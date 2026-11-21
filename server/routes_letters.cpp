// routes_letters.cpp
// Group 3 - Malak
//
// The web addresses the Letters page (student) and the Doctor page call.
// All the rules live in src/services/letter_service.cpp, not here.
//
//   GET  /api/letters               my letter requests (student)
//   POST /api/letters               ask a doctor for a letter (student)
//   GET  /api/doctor/letters        requests waiting for me, oldest first (doctor)
//   POST /api/doctor/letters/<id>   send or decline one request (doctor)

#include "app_context.hpp"

using namespace std;

static crow::json::wvalue letterJson(const LetterRequest& l) {
    crow::json::wvalue j;
    j["id"] = l.id;
    j["studentEmail"] = l.studentEmail;
    j["doctorName"] = l.doctorName;
    j["doctorEmail"] = l.doctorEmail;
    j["status"] = l.status;
    j["requestedAt"] = l.requestedAt;
    j["answeredAt"] = l.answeredAt;
    return j;   // the letter text is NOT sent back to the student: it stays private
}

void registerLetterRoutes(App& app, AppContext& ctx) {

    // GET /api/letters
    CROW_ROUTE(app, "/api/letters").methods("GET"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        Vector<LetterRequest> mine = ctx.letters.lettersOf(email);
        crow::json::wvalue j = crow::json::wvalue::list();
        for (int i = 0; i < mine.size(); i++) j[i] = letterJson(mine[i]);
        return crow::response(200, j);
    });

    // POST /api/letters   (body: doctorName, doctorEmail)
    CROW_ROUTE(app, "/api/letters").methods("POST"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");
        auto body = crow::json::load(req.body);
        if (!body) return jsonError(400, "The request is not valid JSON.");

        lock_guard<mutex> guard(ctx.lock);
        // The student's name for the email, from her application if she started one.
        string studentName;
        Application* a = ctx.applications.find(email);
        if (a != nullptr && !a->firstName.empty()) studentName = a->firstName + " " + a->surname;

        string doctorEmail = jsonText(body, "doctorEmail");
        for (char& c : doctorEmail) c = (char)tolower((unsigned char)c);

        string error;
        int id = ctx.letters.requestLetter(email, jsonText(body, "doctorName"), doctorEmail,
                                           nowText(), error, studentName);
        if (id == -1) return jsonError(409, error);

        crow::json::wvalue j;
        j["id"] = id;
        return crow::response(201, j);
    });

    // GET /api/doctor/letters   (the signed-in doctor's queue, oldest first)
    CROW_ROUTE(app, "/api/doctor/letters").methods("GET"_method)
    ([&ctx](const crow::request& req) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");

        lock_guard<mutex> guard(ctx.lock);
        Vector<LetterRequest> pending = ctx.letters.pendingFor(email);
        crow::json::wvalue j = crow::json::wvalue::list();
        for (int i = 0; i < pending.size(); i++) {
            crow::json::wvalue item = letterJson(pending[i]);
            // Add the student's details so the doctor knows who is asking (HashMap lookup).
            Application* a = ctx.applications.find(pending[i].studentEmail);
            item["studentName"] = (a != nullptr) ? a->firstName + " " + a->surname : "";
            item["studentMajor"] = (a != nullptr) ? a->major : "";
            item["studentGpa"] = (a != nullptr) ? a->gpa : 0.0;
            j[i] = std::move(item);
        }
        return crow::response(200, j);
    });

    // POST /api/doctor/letters/<id>   (body: send true/false, letterText)
    CROW_ROUTE(app, "/api/doctor/letters/<int>").methods("POST"_method)
    ([&ctx](const crow::request& req, int id) {
        string email = currentUser(req);
        if (email.empty()) return jsonError(401, "Sign in with your AUC account.");
        auto body = crow::json::load(req.body);
        if (!body) return jsonError(400, "The request is not valid JSON.");

        lock_guard<mutex> guard(ctx.lock);
        string error;
        if (!ctx.letters.answer(id, email, jsonBool(body, "send"), jsonText(body, "letterText"),
                                nowText(), error))
            return jsonError(409, error);

        crow::json::wvalue j;
        j["ok"] = true;
        return crow::response(200, j);
    });
}
