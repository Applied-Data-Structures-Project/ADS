// routes_application.cpp
// The web addresses the application pages call. Each route is thin:
//   1. who is calling?   2. read the JSON   3. ask the service   4. answer with JSON
// All the rules live in src/services/application_service.cpp, not here.
//
//   GET    /api/application                    my application + documents + what is missing
//   PUT    /api/application                    save my draft
//   POST   /api/application/eligibility        check the eligibility rules
//   POST   /api/application/submit             submit (final)
//   PUT    /api/application/documents/<type>   upload one PDF (the request body is the file)
//   DELETE /api/application/documents/<type>   remove one PDF
//   GET    /api/admin/review                   the review queue (for IPSO / the admin side)

#include "app_context.hpp"

#include <filesystem>
#include <fstream>

using namespace std;
namespace fs = std::filesystem;

static const char* DOC_TYPES[4] = {"courses", "transcript", "cv", "passport"};
static const size_t MAX_PDF_BYTES = 5 * 1024 * 1024;   // IPSO: PDF, at most 5 MB

// ---------- Application <-> JSON ----------
// Same field names as frontend/src/lib/student/types.ts (camelCase).

static crow::json::wvalue applicationJson(const Application& a) {
    crow::json::wvalue j;
    j["studentEmail"] = a.studentEmail;
    j["firstName"] = a.firstName;
    j["surname"] = a.surname;
    j["dateOfBirth"] = a.dateOfBirth;
    j["nationality"] = a.nationality;
    j["secondNationality"] = a.secondNationality;
    j["gender"] = a.gender;
    j["phone"] = a.phone;
    j["aucId"] = a.aucId;
    j["gpa"] = a.gpa;
    j["level"] = a.level;
    j["major"] = a.major;
    j["rhet1020"] = a.rhet1020;
    j["expectedGraduation"] = a.expectedGraduation;
    j["accommodation"] = a.accommodation;
    j["scholarship"] = a.scholarship;
    j["programType"] = a.programType;
    for (int i = 0; i < 3; i++) j["pref"][i] = a.pref[i];   // university slugs, "" = not chosen
    j["guardianName"] = a.guardianName;
    j["guardianRelation"] = a.guardianRelation;
    j["guardianPhone"] = a.guardianPhone;
    j["guardianEmail"] = a.guardianEmail;
    j["guardianLanguage"] = a.guardianLanguage;
    j["qPreparedness"] = a.qPreparedness;
    j["qGoals"] = a.qGoals;
    j["travelExperience"] = a.travelExperience;
    j["qAdaptability"] = a.qAdaptability;
    j["qTeamwork"] = a.qTeamwork;
    j["qReflection"] = a.qReflection;
    j["qFinance"] = a.qFinance;
    j["budget"] = a.budget;
    j["prior"] = a.prior;
    j["declaration"] = a.declaration;
    j["status"] = a.status;
    if (a.goodStanding < 0) j["goodStanding"] = nullptr;   // not checked by IPSO yet
    else j["goodStanding"] = (a.goodStanding == 1);
    j["submittedAt"] = a.submittedAt;
    j["createdAt"] = a.createdAt;
    j["updatedAt"] = a.updatedAt;
    return j;
}

// Only the fields a student may fill. status, dates, prior and goodStanding are
// decided by the server, so they are ignored even if the page sends them.
static Application applicationFromJson(const crow::json::rvalue& b, const string& email) {
    Application a;
    a.studentEmail = email;
    a.firstName = jsonText(b, "firstName");
    a.surname = jsonText(b, "surname");
    a.dateOfBirth = jsonText(b, "dateOfBirth");
    a.nationality = jsonText(b, "nationality");
    a.secondNationality = jsonText(b, "secondNationality");
    a.gender = jsonText(b, "gender");
    a.phone = jsonText(b, "phone");
    a.aucId = jsonText(b, "aucId");
    a.gpa = jsonNumber(b, "gpa");
    a.level = jsonText(b, "level");
    a.major = jsonText(b, "major");
    a.rhet1020 = jsonText(b, "rhet1020");
    a.expectedGraduation = jsonText(b, "expectedGraduation");
    a.accommodation = jsonBool(b, "accommodation");
    a.scholarship = jsonText(b, "scholarship");
    a.programType = jsonText(b, "programType");
    if (b.has("pref") && b["pref"].t() == crow::json::type::List) {
        for (int i = 0; i < 3 && i < (int)b["pref"].size(); i++) {
            if (b["pref"][i].t() == crow::json::type::String) a.pref[i] = string(b["pref"][i].s());
        }
    }
    a.guardianName = jsonText(b, "guardianName");
    a.guardianRelation = jsonText(b, "guardianRelation");
    a.guardianPhone = jsonText(b, "guardianPhone");
    a.guardianEmail = jsonText(b, "guardianEmail");
    a.guardianLanguage = jsonText(b, "guardianLanguage");
    a.qPreparedness = jsonText(b, "qPreparedness");
    a.qGoals = jsonText(b, "qGoals");
    a.travelExperience = jsonText(b, "travelExperience");
    a.qAdaptability = jsonText(b, "qAdaptability");
    a.qTeamwork = jsonText(b, "qTeamwork");
    a.qReflection = jsonText(b, "qReflection");
    a.qFinance = jsonText(b, "qFinance");
    a.budget = jsonInt(b, "budget");
    a.declaration = jsonBool(b, "declaration");
    return a;
}

static bool validDocType(const string& t) {
    for (int i = 0; i < 4; i++) if (t == DOC_TYPES[i]) return true;
    return false;
}

// "salma@aucegypt.edu" -> "salma_aucegypt.edu", safe as a folder name.
static string folderName(const string& email) {
    string out;
    for (char c : email) out += (isalnum((unsigned char)c) || c == '.' || c == '-') ? c : '_';
    return out;
}

// Everything the application pages need in one answer.
static crow::json::wvalue fullState(AppContext& ctx, const string& email) {
    crow::json::wvalue j;
    Application* a = ctx.applications.find(email);
    if (a == nullptr) j["application"] = nullptr;   // not started yet
    else j["application"] = applicationJson(*a);

    int n = 0;
    j["documents"] = crow::json::wvalue::list();
    for (int i = 0; i < 4; i++) {
        if (ctx.applications.hasDocument(email, DOC_TYPES[i])) j["documents"][n++] = DOC_TYPES[i];
    }

    bool letter = ctx.letters.hasReceivedLetter(email);
    j["letterReceived"] = letter;
    Vector<string> todo = ctx.applications.missing(email, letter);
    j["missing"] = crow::json::wvalue::list();
    for (int i = 0; i < todo.size(); i++) j["missing"][i] = todo[i];
    return j;
}

void registerApplicationRoutes(App& app, AppContext& ctx) {

    // GET /api/application
    CROW_ROUTE(app, "/api/application").methods("GET"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");
        lock_guard<mutex> guard(ctx.lock);
        return crow::response(200, fullState(ctx, email));
    });

    // PUT /api/application   (body: the application as JSON)
    CROW_ROUTE(app, "/api/application").methods("PUT"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");
        auto body = crow::json::load(req.body);
        if (!body) return jsonError(400, "The request is not valid JSON.");

        lock_guard<mutex> guard(ctx.lock);
        string error;
        if (!ctx.applications.saveDraft(applicationFromJson(body, email), nowText(), error))
            return jsonError(409, error);
        return crow::response(200, fullState(ctx, email));
    });

    // POST /api/application/eligibility
    // Uses the student's saved /api/me eligibility profile in O(1) when present;
    // falls back to the request body only for callers that have not saved a profile yet.
    CROW_ROUTE(app, "/api/application/eligibility").methods("POST"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        const StudentRecord* rec = ctx.students.find(email);
        if (rec != nullptr && rec->eligibilitySaved) {
            StudentEligibilityOutcome outcome = StudentService::evaluateEligibility(*rec);
            string reason = StudentService::checkEligibilityReason(*rec);
            crow::json::wvalue j;
            j["eligible"] = outcome.eligible;
            j["provisional"] = outcome.provisional;
            j["reason"] = reason;
            return crow::response(200, j);
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("level")) {
            crow::json::wvalue j;
            j["eligible"] = false;
            j["provisional"] = false;
            j["reason"] = "Complete your eligibility profile first.";
            return crow::response(200, j);
        }

        StudentInfo s;
        s.email = email;
        s.level = jsonText(body, "level");
        s.declared = jsonBool(body, "declared");
        s.credits = jsonInt(body, "credits");
        s.gpa = jsonNumber(body, "gpa");

        string reason = ctx.applications.checkEligibility(s);
        crow::json::wvalue j;
        j["eligible"] = reason.empty();
        j["provisional"] = false;
        j["reason"] = reason;
        return crow::response(200, j);
    });

    // POST /api/application/submit
    // Checks full IPSO eligibility from the student's saved /api/me record in O(1),
    // then verifies the deadline, required fields, documents and letter before locking.
    CROW_ROUTE(app, "/api/application/submit").methods("POST"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        string reason;
        const StudentRecord* rec = ctx.students.find(email);
        if (rec != nullptr && rec->eligibilitySaved) {
            reason = StudentService::checkEligibilityReason(*rec);
        } else {
            auto body = crow::json::load(req.body);
            if (body && body.has("level")) {
                StudentInfo s;
                s.email = email;
                s.level = jsonText(body, "level");
                s.declared = jsonBool(body, "declared");
                s.credits = jsonInt(body, "credits");
                s.gpa = jsonNumber(body, "gpa");
                reason = ctx.applications.checkEligibility(s);
            } else {
                reason = "Complete your eligibility profile before submitting.";
            }
        }
        if (!reason.empty()) return jsonError(403, reason);

        string now = nowText();
        string error;
        bool letter = ctx.letters.hasReceivedLetter(email);
        if (!ctx.applications.submit(email, letter, now, error)) return jsonError(409, error);

        string ignored;
        ctx.discovery.notify(
            email,
            "Your Fall 2027 exchange application has been submitted for IPSO review.",
            now, ignored);
        return crow::response(200, fullState(ctx, email));
    });

    // PUT /api/application/documents/<type>   (body: the PDF file itself)
    CROW_ROUTE(app, "/api/application/documents/<string>").methods("PUT"_method)
    ([&ctx](const crow::request& req, const string& docType) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");
        if (!validDocType(docType)) return jsonError(404, "Unknown document type.");
        if (req.body.empty()) return jsonError(400, "The file is empty.");
        if (req.body.size() > MAX_PDF_BYTES) return jsonError(413, "The file is bigger than 5 MB.");
        if (req.body.compare(0, 5, "%PDF-") != 0) return jsonError(415, "Only PDF files are accepted.");

        lock_guard<mutex> guard(ctx.lock);
        Application* a = ctx.applications.find(email);
        if (a != nullptr && a->status != "Draft")
            return jsonError(409, "Your application is submitted, so files can no longer change.");

        // Save the file: uploads/salma_aucegypt.edu/cv.pdf
        fs::path folder = fs::path(ctx.uploadDir) / folderName(email);
        error_code ec;
        fs::create_directories(folder, ec);
        fs::path file = folder / (docType + ".pdf");
        ofstream out(file, ios::binary);
        out.write(req.body.data(), (streamsize)req.body.size());
        out.close();
        if (!out) return jsonError(500, "Could not save the file. Please try again.");

        string error;
        if (!ctx.applications.uploadDocument(email, docType, file.string(), nowText(), error))
            return jsonError(409, error);
        return crow::response(200, fullState(ctx, email));
    });

    // DELETE /api/application/documents/<type>
    CROW_ROUTE(app, "/api/application/documents/<string>").methods("DELETE"_method)
    ([&ctx](const crow::request& req, const string& docType) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");
        if (!validDocType(docType)) return jsonError(404, "Unknown document type.");

        lock_guard<mutex> guard(ctx.lock);
        string error;
        if (!ctx.applications.removeDocument(email, docType, error))
            return jsonError(409, error.empty() ? "Could not remove the file." : error);
        error_code ec;
        fs::remove(fs::path(ctx.uploadDir) / folderName(email) / (docType + ".pdf"), ec);
        return crow::response(200, fullState(ctx, email));
    });

    // GET /api/admin/review   (who is first in IPSO's review queue)
    CROW_ROUTE(app, "/api/admin/review").methods("GET"_method)
    ([&ctx]() {
        lock_guard<mutex> guard(ctx.lock);
        crow::json::wvalue j;
        j["waiting"] = ctx.applications.waitingForReview();
        string next = ctx.applications.nextForReview();
        if (next.empty()) j["next"] = nullptr;
        else j["next"] = applicationJson(*ctx.applications.find(next));
        return crow::response(200, j);
    });
}
