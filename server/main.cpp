// ADS Exchange Matcher - REST API entry point.
//
// This is the ONLY layer that knows about Crow / JSON. Keep route handlers thin:
//   parse request -> call a service in src/services -> serialize the result.
//
// GET /api/health (setup verification), plus Group 3's routes in routes_application.cpp
// and routes_letters.cpp.
// Config (env vars): PORT (default 18080), DB_PATH (default <repo>/db/app.db),
//   UPLOAD_DIR (default <repo>/uploads), SITE_URL (default http://localhost:5173),
//   SMTP_USER / SMTP_PASS for real emails (relayed via SvelteKit, see smtp_notifier.hpp).
//   (GROQ_API_KEY and SMTP TLS are handled directly by SvelteKit's Node.js server routes.)

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "sqlite3.h"


#include "app_context.hpp"
#include "smtp_notifier.hpp"
#include "repo/admin_repo.hpp"
#include "repo/application_repo.hpp"
#include "repo/course_plan_repo.hpp"
#include "repo/discovery_repo.hpp"
#include "repo/student_repo.hpp"
#include "services/admin_service.hpp"
#include "services/application_service.hpp"
#include "services/course_plan_service.hpp"
#include "services/discovery_service.hpp"
#include "services/letter_service.hpp"
#include "services/student_service.hpp"
#include "vector.hpp"

#include "repo/university_repo.hpp"
#include "services/university_service.hpp"

#include <utility>


namespace {

// Opens (or creates) the SQLite database and checks it answers a trivial query.
bool checkDatabase(const std::string& path, std::string& versionOut) {
    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, "SELECT sqlite_version();", -1, &stmt, nullptr) == SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW) {
        versionOut = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        ok = true;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

std::string envOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return (value && *value) ? std::string(value) : fallback;
}

// Reads the team's private settings file (<repo>/.env), one KEY=value per line, e.g.
//   GROQ_API_KEY=...
//   SMTP_USER=aucreccomendationsystem@gmail.com
// The file is in .gitignore: it is shared privately, never on GitHub.
// A setting already given in the terminal (export ...) wins over the file.
void loadEnvFile(const std::filesystem::path& file) {
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();   // Windows line endings
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string name = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);
        if (std::getenv(name.c_str()) != nullptr) continue;
#ifdef _WIN32
        _putenv_s(name.c_str(), value.c_str());
#else
        setenv(name.c_str(), value.c_str(), 0);
#endif
    }
}

// Runs every .sql file in a folder, in name order (001_..., 002_..., ...).
// Our migrations use CREATE TABLE IF NOT EXISTS, so running them again is safe.
bool runSqlFolder(sqlite3* db, const std::filesystem::path& folder) {
    Vector<std::string> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (entry.path().extension() == ".sql") files.push_back(entry.path().string());
    }
    for (int i = 1; i < files.size(); i++) {   // insertion sort by name
        std::string current = files[i];
        int j = i - 1;
        while (j >= 0 && files[j] > current) { files[j + 1] = files[j]; j--; }
        files[j + 1] = current;
    }
    for (int i = 0; i < files.size(); i++) {
        std::ifstream in(files[i]);
        std::stringstream sql;
        sql << in.rdbuf();
        char* err = nullptr;
        if (sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            CROW_LOG_ERROR << "SQL file " << files[i] << " failed: " << (err ? err : "?");
            sqlite3_free(err);
            return false;
        }
        CROW_LOG_INFO << "Ran " << files[i];
    }
    return true;
}

// True if the universities table is missing or empty (a fresh database).
bool needsSeed(sqlite3* db) {
    sqlite3_stmt* stmt = nullptr;
    bool empty = true;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM universities", -1, &stmt, nullptr) == SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW) {
        empty = sqlite3_column_int(stmt, 0) == 0;
    }
    sqlite3_finalize(stmt);
    return empty;
}

} // namespace

int main() {
    const std::filesystem::path repoRoot =
        std::filesystem::path(ADS_DEFAULT_DB_PATH).parent_path().parent_path();
    loadEnvFile(repoRoot / ".env");   // the team's Groq key and email settings

    const std::string dbPath = envOr("DB_PATH", ADS_DEFAULT_DB_PATH);
    const int port = std::atoi(envOr("PORT", "18080").c_str());

    // ---- Database: opened ONCE, shared by every request ----
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        CROW_LOG_CRITICAL << "Cannot open the database " << dbPath;
        return 1;
    }
    if (!runSqlFolder(db, repoRoot / "db" / "migrations")) return 1;
    if (needsSeed(db)) runSqlFolder(db, repoRoot / "db" / "seed");

    // ---- Group 2 & 3: load everything into our data structures once ----
    ApplicationRepo repo(db);
    StudentRepo studentRepo(db);
    DiscoveryRepo discoveryRepo(db);
    CoursePlanRepo coursePlanRepo(db);
    UniversityRepo universityRepo(db);
    ApplicationService applications(repo);
    LetterService letters(repo);
    StudentService students(studentRepo);
    DiscoveryService discovery(discoveryRepo);
    CoursePlanService coursePlans(coursePlanRepo);
    UniversityService universityService(universityRepo);
    applications.loadAll();
    letters.loadAll();
    students.loadAll();
    discovery.loadAll();
    coursePlans.loadAll();
    if (!universityService.loadAll()) {
        CROW_LOG_ERROR << "Could not load universities";
        sqlite3_close(db);
        return 1;
    }
    CROW_LOG_INFO << "Universities loaded: " << universityService.count();

    SmtpNotifier smtp;   // real emails if SMTP_USER/SMTP_PASS are set, otherwise printed
    letters.setNotifier(&smtp, envOr("SITE_URL", "http://localhost:5173"), "30 November");

    AdminRepo adminRepo(db);
    AdminService adminService(adminRepo, applications, discovery);
    adminService.loadAll();

    AppContext ctx(applications, letters, students, discovery, universityService, coursePlans, adminService,
                   envOr("UPLOAD_DIR", (repoRoot / "uploads").string()));

    App app;

    // Allow the Svelte dev server to call us directly if not using the Vite proxy.
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .origin("*")
        .methods("GET"_method, "POST"_method, "PUT"_method, "DELETE"_method, "OPTIONS"_method)
        .headers("Content-Type", "Authorization", "X-User-Email");


    // API route that returns all universities to the frontend.
    //
    // When the frontend sends a GET request to /api/universities,
    // Crow runs the code inside this route.

    CROW_ROUTE(app, "/api/universities")
    ([&universityService](const crow::request& req) {

        // Read the search values from the URL.
               // If a value was not provided, it will be nullptr.
               const char* nameParam = req.url_params.get("name");
               const char* countryParam = req.url_params.get("country");
               const char* regionParam = req.url_params.get("region");

               // Convert the values into normal C++ strings.
               // An empty string means that filter is not used.
               std::string name = nameParam ? nameParam : "";
               std::string country = countryParam ? countryParam : "";
               std::string region = regionParam ? regionParam : "";

               // Ask our service to search the universities.
               // The service uses our custom Vector.
               Vector<University> universities =
                   universityService.search(name, country, region);

               // Create the JSON response for the frontend.
               crow::json::wvalue response;

               // Add the number of matching universities.
               response["count"] = universities.size();

               // Create a JSON list to store the university records.
               crow::json::wvalue::list universityList;

               // Go through the matching universities.
               for (int i = 0; i < universities.size(); i++) {

                   // Get the current university.
                   University u = universities[i];

                   // Each university has detailed information stored in metadataJson inside the SQLite database.
                   // This includes academic requirements, housing, semesters, source information, and other details.
                   crow::json::rvalue metadata =
                       crow::json::load(u.metadataJson);

                   // Make sure the university data is valid.
                   if (!metadata ||
                       metadata.t() != crow::json::type::Object) {

                       crow::json::wvalue error;
                       error["message"] = "Invalid university data";

                       return crow::response(500, error);
                   }

                   // Convert the stored information into JSON.
                   crow::json::wvalue item(metadata);

                   // Add the complete university to our JSON list.
                   universityList.push_back(std::move(item));
               }

               // Add the list to the response.
               response["universities"] = std::move(universityList);

               // Send the matching universities to the frontend.
               return crow::response(200, response);
           });
    

    CROW_ROUTE(app, "/api/universities/<string>")
       ([&universityService](std::string slug) {
           // Ask the UniversityService to find this university.
                  // The service uses our custom HashMap to find the university's position inside the Vector.
                  // We use a pointer because the university might not exist in our database.

                  const University* university =
                      universityService.find(slug);
           if (university == nullptr) {

                       crow::json::wvalue error;

                       error["message"] = "University not found";

                       return crow::response(404, error);
                   }
         
           // All university details are already stored in metadataJson inside our SQLite database.
           // Instead of adding every field manually, we read the complete JSON data and send it to the frontend.

           crow::json::rvalue metadata =
               crow::json::load(university->metadataJson);

           // Check whether SQLite's stored metadata contains valid JSON.
           // If the data is invalid, return an error instead of sending incorrect university information.

           if (!metadata || metadata.t() != crow::json::type::Object) {

               crow::json::wvalue error;
               error["message"] = "University details are unavailable";

               return crow::response(500, error);
           }

           // Convert the stored JSON into a response object.
           // This includes the university name, country, GPA, available semesters, housing, eligible majors, course catalog, and other details.

           crow::json::wvalue response(metadata);

           // Send the complete university information to the frontend.
           // HTTP 200 means the request was successful.

           return crow::response(200, response);

              });
    
    CROW_ROUTE(app, "/api/health")
    ([&dbPath]() {
        std::string sqliteVersion;
        bool dbOk = checkDatabase(dbPath, sqliteVersion);

        crow::json::wvalue body;
        body["status"] = "ok";
        body["db"] = dbOk ? "ok" : "error";
        body["sqlite_version"] = sqliteVersion;
        return crow::response(dbOk ? 200 : 500, body);
    });

    registerApplicationRoutes(app, ctx);
    registerLetterRoutes(app, ctx);
    registerStudentRoutes(app, ctx);
    registerDiscoveryRoutes(app, ctx);
    registerRecommendationRoutes(app, ctx);
    registerCoursePlanRoutes(app, ctx);
    registerAdminRoutes(app, ctx);

    CROW_LOG_INFO << "Database: " << dbPath;
    app.port(port).multithreaded().run();

    sqlite3_close(db);
    return 0;
}

