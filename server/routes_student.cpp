// routes_student.cpp
// Student profile, eligibility intake, and semester budget routes.
//
//   GET /api/me   my student profile + eligibility outcome (profile is null until saved once)
//   PUT /api/me   create or partially update my student profile / eligibility / budget

#include "app_context.hpp"

using namespace std;

static crow::json::wvalue studentJson(const StudentRecord& r) {
    crow::json::wvalue j;
    j["email"] = r.email;
    j["name"] = r.name;
    j["aucId"] = r.aucId;
    j["phone"] = r.phone;
    j["level"] = r.level;
    j["major"] = r.major;
    if (r.hasGpa) j["gpa"] = r.gpa;
    else j["gpa"] = nullptr;
    if (r.hasCredits) j["credits"] = r.credits;
    else j["credits"] = nullptr;
    j["majorDeclared"] = r.majorDeclared;
    j["rhet110"] = r.rhet110;
    j["rhet120"] = r.rhet120;
    j["finalSemester"] = r.finalSemester;
    j["academicProbation"] = r.academicProbation;
    j["disciplinaryProbation"] = r.disciplinaryProbation;
    j["englishComplete"] = r.englishComplete;
    j["firstSemesterNoGpa"] = r.firstSemesterNoGpa;
    j["eligibilitySaved"] = r.eligibilitySaved;
    j["budget"] = r.budget;
    j["hasBudget"] = r.hasBudget;
    j["updatedAt"] = r.updatedAt;
    return j;
}

static crow::json::wvalue meResponse(const StudentRecord* r) {
    crow::json::wvalue body;
    if (r == nullptr) {
        body["profile"] = nullptr;
        body["eligibility"]["eligible"] = false;
        body["eligibility"]["provisional"] = false;
        body["eligibility"]["reason"] = "Complete your eligibility profile first.";
    } else {
        body["profile"] = studentJson(*r);
        StudentEligibilityOutcome outcome = StudentService::evaluateEligibility(*r);
        body["eligibility"]["eligible"] = outcome.eligible;
        body["eligibility"]["provisional"] = outcome.provisional;
        body["eligibility"]["reason"] = StudentService::checkEligibilityReason(*r);
    }
    return body;
}

static void applyPatch(StudentRecord& r, const crow::json::rvalue& b) {
    if (b.has("name") && b["name"].t() == crow::json::type::String) r.name = string(b["name"].s());
    if (b.has("aucId") && b["aucId"].t() == crow::json::type::String) r.aucId = string(b["aucId"].s());
    if (b.has("phone") && b["phone"].t() == crow::json::type::String) r.phone = string(b["phone"].s());
    if (b.has("level") && b["level"].t() == crow::json::type::String) r.level = string(b["level"].s());
    if (b.has("major") && b["major"].t() == crow::json::type::String) r.major = string(b["major"].s());

    if (b.has("gpa")) {
        if (b["gpa"].t() == crow::json::type::Null) {
            r.hasGpa = false;
            r.gpa = 0.0;
        } else if (b["gpa"].t() == crow::json::type::Number) {
            r.hasGpa = true;
            r.gpa = b["gpa"].d();
        }
    }

    if (b.has("credits")) {
        if (b["credits"].t() == crow::json::type::Null) {
            r.hasCredits = false;
            r.credits = 0;
        } else if (b["credits"].t() == crow::json::type::Number) {
            r.hasCredits = true;
            r.credits = static_cast<int>(b["credits"].i());
        }
    }

    if (b.has("majorDeclared")) r.majorDeclared = jsonBool(b, "majorDeclared", r.majorDeclared);
    if (b.has("rhet110")) r.rhet110 = jsonBool(b, "rhet110", r.rhet110);
    if (b.has("rhet120")) r.rhet120 = jsonBool(b, "rhet120", r.rhet120);
    if (b.has("finalSemester")) r.finalSemester = jsonBool(b, "finalSemester", r.finalSemester);
    if (b.has("academicProbation")) r.academicProbation = jsonBool(b, "academicProbation", r.academicProbation);
    if (b.has("disciplinaryProbation")) r.disciplinaryProbation = jsonBool(b, "disciplinaryProbation", r.disciplinaryProbation);
    if (b.has("englishComplete")) r.englishComplete = jsonBool(b, "englishComplete", r.englishComplete);
    if (b.has("firstSemesterNoGpa")) r.firstSemesterNoGpa = jsonBool(b, "firstSemesterNoGpa", r.firstSemesterNoGpa);
    if (b.has("eligibilitySaved")) r.eligibilitySaved = jsonBool(b, "eligibilitySaved", r.eligibilitySaved);

    if (b.has("budget") && b["budget"].t() == crow::json::type::Number) {
        r.budget = static_cast<int>(b["budget"].i());
        r.hasBudget = true;
    }
}

void registerStudentRoutes(App& app, AppContext& ctx) {

    // GET /api/me
    CROW_ROUTE(app, "/api/me").methods("GET"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        const StudentRecord* found = ctx.students.find(email);
        return crow::response(200, meResponse(found));
    });

    // PUT /api/me
    CROW_ROUTE(app, "/api/me").methods("PUT"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");
        auto body = crow::json::load(req.body);
        if (!body) return jsonError(400, "The request is not valid JSON.");

        lock_guard<mutex> guard(ctx.lock);
        StudentRecord next;
        bool wasEligibilitySaved = false;
        if (const StudentRecord* existing = ctx.students.find(email)) {
            next = *existing;
            wasEligibilitySaved = existing->eligibilitySaved;
        }
        next.email = email;
        applyPatch(next, body);

        string now = nowText();
        string error;
        if (!ctx.students.save(next, now, error)) {
            return jsonError(400, error);
        }

        if (!wasEligibilitySaved && next.eligibilitySaved) {
            string ignored;
            ctx.discovery.notify(
                email,
                "Eligibility profile saved. You can now explore universities and apply.",
                now, ignored);
        }

        return crow::response(200, meResponse(ctx.students.find(email)));
    });
}
