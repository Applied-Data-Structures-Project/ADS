// routes_common.cpp
// Group 3 - Malak
//
// Small helpers shared by routes_application.cpp and routes_letters.cpp.

#include "app_context.hpp"

#include <ctime>

using namespace std;

static bool endsWith(const string& text, const string& ending) {
    return text.size() >= ending.size() &&
           text.compare(text.size() - ending.size(), ending.size(), ending) == 0;
}

string currentUser(const crow::request& req) {
    string email = req.get_header_value("X-User-Email");
    for (char& c : email) c = (char)tolower((unsigned char)c);   // Salma@AUCEgypt.edu -> salma@aucegypt.edu
    if (!endsWith(email, "@aucegypt.edu")) return "";
    return email;
}

string nowText() {
    char buf[20];
    time_t t = time(nullptr);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", localtime(&t));
    return buf;
}

crow::response jsonError(int code, const string& message) {
    crow::json::wvalue body;
    body["error"] = message;
    return crow::response(code, body);
}

string jsonText(const crow::json::rvalue& body, const char* key) {
    if (!body.has(key) || body[key].t() != crow::json::type::String) return "";
    return string(body[key].s());
}

bool jsonBool(const crow::json::rvalue& body, const char* key, bool fallback) {
    if (!body.has(key)) return fallback;
    if (body[key].t() == crow::json::type::True) return true;
    if (body[key].t() == crow::json::type::False) return false;
    return fallback;
}

int jsonInt(const crow::json::rvalue& body, const char* key, int fallback) {
    if (!body.has(key) || body[key].t() != crow::json::type::Number) return fallback;
    return (int)body[key].i();
}

double jsonNumber(const crow::json::rvalue& body, const char* key, double fallback) {
    if (!body.has(key) || body[key].t() != crow::json::type::Number) return fallback;
    return body[key].d();
}
