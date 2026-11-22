// routes_discovery.cpp
// Saved universities and FIFO notification queue routes.
//
//   GET    /api/saved                      my saved university slugs (in save order)
//   POST   /api/saved/<slug>               save one university slug
//   DELETE /api/saved/<slug>               remove one university slug
//   GET    /api/notifications              my notifications oldest-first + unreadCount
//   POST   /api/notifications              push one notification to the queue
//   POST   /api/notifications/read-oldest  mark the front (oldest unread) notification read
//   POST   /api/notifications/read-all     mark all unread notifications read

#include "app_context.hpp"

using namespace std;

static crow::json::wvalue savedResponse(const DiscoveryService& discovery, const string& email) {
    Vector<string> slugs = discovery.savedOf(email);
    crow::json::wvalue body;
    body["saved"] = crow::json::wvalue::list();
    for (int i = 0; i < slugs.size(); i++) {
        body["saved"][i] = slugs[i];
    }
    return body;
}

static crow::json::wvalue notificationsResponse(const DiscoveryService& discovery, const string& email) {
    Vector<Notification> list = discovery.notificationsOf(email);
    crow::json::wvalue body;
    body["notifications"] = crow::json::wvalue::list();
    for (int i = 0; i < list.size(); i++) {
        crow::json::wvalue item;
        item["id"] = list[i].id;
        item["message"] = list[i].message;
        item["createdAt"] = list[i].createdAt;
        item["read"] = list[i].read;
        body["notifications"][i] = std::move(item);
    }
    body["unreadCount"] = discovery.unreadCount(email);
    return body;
}

void registerDiscoveryRoutes(App& app, AppContext& ctx) {

    // GET /api/saved
    CROW_ROUTE(app, "/api/saved").methods("GET"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        return crow::response(200, savedResponse(ctx.discovery, email));
    });

    // POST /api/saved/<slug>
    CROW_ROUTE(app, "/api/saved/<string>").methods("POST"_method)
    ([&ctx](const crow::request& req, const string& slug) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        string error;
        if (!ctx.discovery.saveUniversity(email, slug, nowText(), error)) {
            return jsonError(400, error);
        }
        return crow::response(200, savedResponse(ctx.discovery, email));
    });

    // DELETE /api/saved/<slug>
    CROW_ROUTE(app, "/api/saved/<string>").methods("DELETE"_method)
    ([&ctx](const crow::request& req, const string& slug) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        string error;
        if (!ctx.discovery.removeUniversity(email, slug, error)) {
            return jsonError(400, error);
        }
        return crow::response(200, savedResponse(ctx.discovery, email));
    });

    // GET /api/notifications
    CROW_ROUTE(app, "/api/notifications").methods("GET"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        ctx.discovery.ensureWelcomeNotification(email, nowText());
        return crow::response(200, notificationsResponse(ctx.discovery, email));
    });

    // POST /api/notifications   (body: { "message": "..." })
    CROW_ROUTE(app, "/api/notifications").methods("POST"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");
        auto body = crow::json::load(req.body);
        if (!body) return jsonError(400, "The request is not valid JSON.");

        lock_guard<mutex> guard(ctx.lock);
        string error;
        int id = ctx.discovery.notify(email, jsonText(body, "message"), nowText(), error);
        if (id < 0) return jsonError(400, error);

        crow::json::wvalue res = notificationsResponse(ctx.discovery, email);
        res["id"] = id;
        return crow::response(201, res);
    });

    // POST /api/notifications/read-oldest
    CROW_ROUTE(app, "/api/notifications/read-oldest").methods("POST"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        string error;
        int readId = ctx.discovery.markOldestRead(email, error);
        if (readId < 0) return jsonError(500, error);

        crow::json::wvalue res = notificationsResponse(ctx.discovery, email);
        res["readId"] = readId;
        return crow::response(200, res);
    });

    // POST /api/notifications/read-all
    CROW_ROUTE(app, "/api/notifications/read-all").methods("POST"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        string error;
        if (!ctx.discovery.markAllRead(email, error)) {
            return jsonError(500, error);
        }
        return crow::response(200, notificationsResponse(ctx.discovery, email));
    });
}
