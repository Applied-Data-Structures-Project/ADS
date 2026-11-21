// ADS Exchange Matcher - REST API entry point.
//
// This is the ONLY layer that knows about Crow / JSON. Keep route handlers thin:
//   parse request -> call a service in src/services -> serialize the result.
//
// GET /api/health (setup verification), plus Group 3's routes in routes_application.cpp
// and routes_letters.cpp.
// Config (env vars): PORT (default 18080), DB_PATH (default <repo>/db/app.db),
//   UPLOAD_DIR (default <repo>/uploads), SITE_URL (default http://localhost:5173),
//   SMTP_USER / SMTP_PASS for real emails (see smtp_notifier.hpp)

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
#include "repo/application_repo.hpp"
#include "services/application_service.hpp"
#include "services/letter_service.hpp"
#include "vector.hpp"

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
    const std::string dbPath = envOr("DB_PATH", ADS_DEFAULT_DB_PATH);
    const int port = std::atoi(envOr("PORT", "18080").c_str());

    // ---- Database: opened ONCE, shared by every request ----
    const std::filesystem::path repoRoot =
        std::filesystem::path(ADS_DEFAULT_DB_PATH).parent_path().parent_path();
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        CROW_LOG_CRITICAL << "Cannot open the database " << dbPath;
        return 1;
    }
    if (!runSqlFolder(db, repoRoot / "db" / "migrations")) return 1;
    if (needsSeed(db)) runSqlFolder(db, repoRoot / "db" / "seed");

    // ---- Group 3: load everything into our data structures once ----
    ApplicationRepo repo(db);
    ApplicationService applications(repo);
    LetterService letters(repo);
    applications.loadAll();
    letters.loadAll();

    SmtpNotifier smtp;   // real emails if SMTP_USER/SMTP_PASS are set, otherwise printed
    letters.setNotifier(&smtp, envOr("SITE_URL", "http://localhost:5173"), "30 November");

    AppContext ctx(applications, letters, envOr("UPLOAD_DIR", (repoRoot / "uploads").string()));

    App app;

    // Allow the Svelte dev server to call us directly if not using the Vite proxy.
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .origin("*")
        .methods("GET"_method, "POST"_method, "PUT"_method, "DELETE"_method, "OPTIONS"_method)
        .headers("Content-Type", "Authorization", "X-User-Email");

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

    CROW_LOG_INFO << "Database: " << dbPath;
    app.port(port).multithreaded().run();
    sqlite3_close(db);
    return 0;
}

