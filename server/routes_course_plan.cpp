// routes_course_plan.cpp
// Draft List of Courses for Study Abroad routes:
//   GET    /api/course-plans          list all saved course plans for the signed-in student
//   PUT    /api/course-plans/<slug>   save/replace the draft course plan for one host university
//   DELETE /api/course-plans/<slug>   delete the draft course plan for one host university

#include "app_context.hpp"

using namespace std;

static crow::json::wvalue planJson(const CoursePlan& p) {
    crow::json::wvalue j;
    j["universitySlug"] = p.universitySlug;
    j["universityName"] = p.universityName;
    j["bannerInstCode"] = p.bannerInstCode;
    j["bannerInstName"] = p.bannerInstName;
    j["studyTerm"] = p.studyTerm;
    j["updatedAt"] = p.updatedAt;
    j["courses"] = crow::json::wvalue::list();
    for (int i = 0; i < p.courses.size(); i++) {
        crow::json::wvalue c;
        c["hostCourseCode"] = p.courses[i].hostCourseCode;
        c["hostCourseTitle"] = p.courses[i].hostCourseTitle;
        c["aucEquivalent"] = p.courses[i].aucEquivalent;
        c["effectiveTerm"] = p.courses[i].effectiveTerm;
        c["source"] = p.courses[i].source;
        j["courses"][i] = std::move(c);
    }
    return j;
}

static crow::json::wvalue allPlansJson(const CoursePlanService& svc, const string& email) {
    Vector<CoursePlan> plans = svc.plansOf(email);
    crow::json::wvalue body;
    body["plans"] = crow::json::wvalue::list();
    for (int i = 0; i < plans.size(); i++) {
        body["plans"][i] = planJson(plans[i]);
    }
    return body;
}

void registerCoursePlanRoutes(App& app, AppContext& ctx) {

    // GET /api/course-plans
    CROW_ROUTE(app, "/api/course-plans").methods("GET"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        return crow::response(200, allPlansJson(ctx.coursePlans, email));
    });

    // PUT /api/course-plans/<slug>
    CROW_ROUTE(app, "/api/course-plans/<string>").methods("PUT"_method)
    ([&ctx](const crow::request& req, const string& slug) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        auto body = crow::json::load(req.body);
        if (!body) return jsonError(400, "The request is not valid JSON.");

        CoursePlan input;
        input.studentEmail = email;
        input.universitySlug = slug;
        input.universityName = jsonText(body, "universityName");
        input.bannerInstCode = jsonText(body, "bannerInstCode");
        input.bannerInstName = jsonText(body, "bannerInstName");
        input.studyTerm = jsonText(body, "studyTerm");

        if (body.has("courses") && body["courses"].t() == crow::json::type::List) {
            const auto& list = body["courses"];
            for (size_t i = 0; i < list.size(); i++) {
                if (list[i].t() != crow::json::type::Object) continue;
                PlannedCourse c;
                c.hostCourseCode = jsonText(list[i], "hostCourseCode");
                c.hostCourseTitle = jsonText(list[i], "hostCourseTitle");
                c.aucEquivalent = jsonText(list[i], "aucEquivalent");
                c.effectiveTerm = jsonText(list[i], "effectiveTerm");
                c.source = jsonText(list[i], "source");
                input.courses.push_back(c);
            }
        }

        lock_guard<mutex> guard(ctx.lock);
        if (input.universityName.empty()) {
            if (const University* u = ctx.universities.find(slug)) {
                input.universityName = u->name;
            }
        }

        string error;
        if (!ctx.coursePlans.savePlan(input, nowText(), error)) {
            return jsonError(400, error);
        }

        crow::json::wvalue res = allPlansJson(ctx.coursePlans, email);
        if (const CoursePlan* saved = ctx.coursePlans.find(email, slug)) {
            res["plan"] = planJson(*saved);
        }
        return crow::response(200, res);
    });

    // DELETE /api/course-plans/<slug>
    CROW_ROUTE(app, "/api/course-plans/<string>").methods("DELETE"_method)
    ([&ctx](const crow::request& req, const string& slug) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        string error;
        if (!ctx.coursePlans.deletePlan(email, slug, error)) {
            return jsonError(400, error);
        }
        return crow::response(200, allPlansJson(ctx.coursePlans, email));
    });
}
