// routes_recommendations.cpp
// Top-K partner university recommendations computed by C++ Max-Heap (include/heap.hpp).
//
//   GET /api/recommendations?k=3   top K unsaved partner universities ranked by fit score

#include "app_context.hpp"

#include <cstdlib>
#include "services/recommender_service.hpp"

using namespace std;

void registerRecommendationRoutes(App& app, AppContext& ctx) {

    // GET /api/recommendations?k=3
    CROW_ROUTE(app, "/api/recommendations").methods("GET"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        int k = 3;
        const char* kParam = req.url_params.get("k");
        if (kParam != nullptr && *kParam != '\0') {
            int parsed = atoi(kParam);
            if (parsed >= 1 && parsed <= 20) k = parsed;
        }

        lock_guard<mutex> guard(ctx.lock);
        StudentRecord student;
        if (const StudentRecord* found = ctx.students.find(email)) {
            student = *found;
        } else {
            student.email = email;
        }

        Vector<UniversityRecommendation> top = RecommenderService::recommend(
            student, ctx.universities.all(), &ctx.discovery, k);

        crow::json::wvalue body;
        body["algorithm"] = "cpp-max-heap";
        body["count"] = top.size();
        body["recommendations"] = crow::json::wvalue::list();
        for (int i = 0; i < top.size(); i++) {
            crow::json::wvalue item;
            item["slug"] = top[i].slug;
            item["name"] = top[i].name;
            item["country"] = top[i].country;
            item["city"] = top[i].city;
            item["region"] = top[i].region;
            item["fitScore"] = top[i].fitScore;
            item["eligible"] = top[i].eligible;
            item["provisional"] = top[i].provisional;
            item["eligibilityReason"] = top[i].eligibilityReason;
            if (top[i].minGpaHost > 0.0) item["minGpaHost"] = top[i].minGpaHost;
            else item["minGpaHost"] = nullptr;
            item["estimatedSemesterCostUsd"] = top[i].estimatedSemesterCostUsd;
            item["withinBudget"] = top[i].withinBudget;
            item["deadline"] = top[i].deadline;
            item["matchSummary"] = top[i].matchSummary;
            body["recommendations"][i] = std::move(item);
        }

        return crow::response(200, body);
    });
}
