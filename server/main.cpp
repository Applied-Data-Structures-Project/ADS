// ADS Exchange Matcher - REST API entry point.
//
// This is the ONLY layer that knows about Crow / JSON. Keep route handlers thin:
//   parse request -> call a service in src/services -> serialize the result.
//
// Currently implements only GET /api/health (setup verification).
// Config (env vars): PORT (default 18080), DB_PATH (default <repo>/db/app.db)

#include <cstdlib>
#include <string>

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "sqlite3.h"

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

} // namespace

int main() {
    const std::string dbPath = envOr("DB_PATH", ADS_DEFAULT_DB_PATH);
    const int port = std::atoi(envOr("PORT", "18080").c_str());

    crow::App<crow::CORSHandler> app;

    // Allow the Svelte dev server to call us directly if not using the Vite proxy.
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .origin("*")
        .methods("GET"_method, "POST"_method, "PUT"_method, "DELETE"_method, "OPTIONS"_method)
        .headers("Content-Type", "Authorization");

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

    CROW_LOG_INFO << "Database: " << dbPath;
    app.port(port).multithreaded().run();
    return 0;
}

