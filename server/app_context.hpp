#pragma once

// app_context.hpp
// Group 3 - Malak
//
// What every route file needs: the services, where uploads go, and a few small helpers.
// main.cpp creates ONE AppContext when the server starts and gives it to each route file.

#include <mutex>
#include <string>

#include "crow.h"
#include "crow/middlewares/cors.h"
#include "services/admin_service.hpp"
#include "services/application_service.hpp"
#include "services/discovery_service.hpp"
#include "services/letter_service.hpp"
#include "services/student_service.hpp"

using App = crow::App<crow::CORSHandler>;

struct AppContext {
    ApplicationService& applications;
    LetterService& letters;
    StudentService& students;
    DiscoveryService& discovery;
    AdminService& admin;
    std::string uploadDir;

    // Crow answers requests on several threads at once, but our HashMaps and Queues
    // are not safe for that. Each route locks this first, so one request at a time
    // touches the data structures.
    std::mutex lock;

    AppContext(ApplicationService& a, LetterService& l, StudentService& s,
               DiscoveryService& d, AdminService& adm, const std::string& uploads)
        : applications(a), letters(l), students(s), discovery(d), admin(adm), uploadDir(uploads) {}
};

// Each route file adds its routes to the app.
void registerApplicationRoutes(App& app, AppContext& ctx);
void registerLetterRoutes(App& app, AppContext& ctx);
void registerStudentRoutes(App& app, AppContext& ctx);
void registerDiscoveryRoutes(App& app, AppContext& ctx);
void registerAdminRoutes(App& app, AppContext& ctx);

class GroqCoach;
void registerCoachRoutes(App& app, GroqCoach& coach);

// ---------- helpers shared by the route files (routes_common.cpp) ----------

// Who is calling: the AUC email in the X-User-Email header, or "" if missing/not AUC.
// For the course project the front end sends the signed-in email. A real deployment
// would check Microsoft's sign-in token here instead.
std::string currentUser(const crow::request& req);

// The time now, as "2026-11-21T14:05:00" (the format the services compare).
std::string nowText();

// {"error": "..."} with the given HTTP status code.
crow::response jsonError(int code, const std::string& message);

// Read one field from a JSON body, or a default if it is missing / the wrong type.
std::string jsonText(const crow::json::rvalue& body, const char* key);
bool jsonBool(const crow::json::rvalue& body, const char* key, bool fallback = false);
int jsonInt(const crow::json::rvalue& body, const char* key, int fallback = 0);
double jsonNumber(const crow::json::rvalue& body, const char* key, double fallback = 0.0);
