// application_repo.cpp

// Loads and saves applications, documents and letters.
// Every function follows the same 4 SQLite steps:
//   1. prepare  - turn the SQL text into a statement
//   2. bind     - fill in each ? (numbered from 1)
//   3. step     - run it (for SELECT: once per row)
//   4. finalize - free the statement

#include "repo/application_repo.hpp"

using namespace std;

// helpers 
// Reading: columns are numbered from 0. Empty (NULL) columns come back as "", 0 or -1.

static string colText(sqlite3_stmt* s, int i) {
    const unsigned char* t = sqlite3_column_text(s, i);
    return t ? reinterpret_cast<const char*>(t) : "";
}

static int colInt(sqlite3_stmt* s, int i, int ifNull = 0) {
    if (sqlite3_column_type(s, i) == SQLITE_NULL) return ifNull;
    return sqlite3_column_int(s, i);
}

static double colDouble(sqlite3_stmt* s, int i) {
    return sqlite3_column_double(s, i);
}

// Fills the ? at position i with text. Empty text is saved as NULL (an empty box).
static void bindText(sqlite3_stmt* s, int i, const string& value) {
    if (value.empty()) sqlite3_bind_null(s, i);
    else sqlite3_bind_text(s, i, value.c_str(), -1, SQLITE_TRANSIENT);
}

// ---------- constructor ----------

ApplicationRepo::ApplicationRepo(sqlite3* db) : db(db) {}

// ---------- applications ----------

// Reads every row of `applications` into a Vector, once at start-up.
Vector<Application> ApplicationRepo::loadApplications() {
    Vector<Application> result;

    const char* sql =
        "SELECT student_email, first_name, surname, date_of_birth, nationality, "
        "second_nationality, gender, phone, auc_id, gpa, level, major, rhet1020, "
        "expected_graduation, accommodation, scholarship, program_type, "
        "pref1, pref2, pref3, "
        "guardian_name, guardian_relation, guardian_phone, guardian_email, guardian_language, "
        "q_preparedness, q_goals, travel_experience, q_adaptability, q_teamwork, "
        "q_reflection, q_finance, budget, prior, declaration, status, good_standing, "
        "submitted_at, created_at, updated_at "
        "FROM applications";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;

    // Each SQLITE_ROW is one application.
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Application a;
        a.studentEmail       = colText(stmt, 0);
        a.firstName          = colText(stmt, 1);
        a.surname            = colText(stmt, 2);
        a.dateOfBirth        = colText(stmt, 3);
        a.nationality        = colText(stmt, 4);
        a.secondNationality  = colText(stmt, 5);
        a.gender             = colText(stmt, 6);
        a.phone              = colText(stmt, 7);
        a.aucId              = colText(stmt, 8);
        a.gpa                = colDouble(stmt, 9);
        a.level              = colText(stmt, 10);
        a.major              = colText(stmt, 11);
        a.rhet1020           = colText(stmt, 12);
        a.expectedGraduation = colText(stmt, 13);
        a.accommodation      = colInt(stmt, 14) == 1;
        a.scholarship        = colText(stmt, 15);
        a.programType        = colText(stmt, 16);
        a.pref[0]            = colText(stmt, 17);
        a.pref[1]            = colText(stmt, 18);
        a.pref[2]            = colText(stmt, 19);
        a.guardianName       = colText(stmt, 20);
        a.guardianRelation   = colText(stmt, 21);
        a.guardianPhone      = colText(stmt, 22);
        a.guardianEmail      = colText(stmt, 23);
        a.guardianLanguage   = colText(stmt, 24);
        a.qPreparedness      = colText(stmt, 25);
        a.qGoals             = colText(stmt, 26);
        a.travelExperience   = colText(stmt, 27);
        a.qAdaptability      = colText(stmt, 28);
        a.qTeamwork          = colText(stmt, 29);
        a.qReflection        = colText(stmt, 30);
        a.qFinance           = colText(stmt, 31);
        a.budget             = colInt(stmt, 32);
        a.prior              = colInt(stmt, 33) == 1;
        a.declaration        = colInt(stmt, 34) == 1;
        a.status             = colText(stmt, 35);
        a.goodStanding       = colInt(stmt, 36, -1);   // empty = IPSO hasn't checked yet
        a.submittedAt        = colText(stmt, 37);
        a.createdAt          = colText(stmt, 38);
        a.updatedAt          = colText(stmt, 39);
        result.push_back(a);
    }

    sqlite3_finalize(stmt);
    return result;
}

// Saves one application: adds it the first time, replaces it after that.
// Saving: boxes (?) are numbered from 1.
bool ApplicationRepo::saveApplication(const Application& a) {
    const char* sql =
        "INSERT OR REPLACE INTO applications ("
        "student_email, first_name, surname, date_of_birth, nationality, "
        "second_nationality, gender, phone, auc_id, gpa, level, major, rhet1020, "
        "expected_graduation, accommodation, scholarship, program_type, "
        "pref1, pref2, pref3, "
        "guardian_name, guardian_relation, guardian_phone, guardian_email, guardian_language, "
        "q_preparedness, q_goals, travel_experience, q_adaptability, q_teamwork, "
        "q_reflection, q_finance, budget, prior, declaration, status, good_standing, "
        "submitted_at, created_at, updated_at) "
        "VALUES (?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, "
        "?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    bindText(stmt, 1,  a.studentEmail);
    bindText(stmt, 2,  a.firstName);
    bindText(stmt, 3,  a.surname);
    bindText(stmt, 4,  a.dateOfBirth);
    bindText(stmt, 5,  a.nationality);
    bindText(stmt, 6,  a.secondNationality);
    bindText(stmt, 7,  a.gender);
    bindText(stmt, 8,  a.phone);
    bindText(stmt, 9,  a.aucId);
    sqlite3_bind_double(stmt, 10, a.gpa);
    bindText(stmt, 11, a.level);
    bindText(stmt, 12, a.major);
    bindText(stmt, 13, a.rhet1020);
    bindText(stmt, 14, a.expectedGraduation);
    sqlite3_bind_int(stmt, 15, a.accommodation ? 1 : 0);
    bindText(stmt, 16, a.scholarship);
    bindText(stmt, 17, a.programType);
    bindText(stmt, 18, a.pref[0]);
    bindText(stmt, 19, a.pref[1]);
    bindText(stmt, 20, a.pref[2]);
    bindText(stmt, 21, a.guardianName);
    bindText(stmt, 22, a.guardianRelation);
    bindText(stmt, 23, a.guardianPhone);
    bindText(stmt, 24, a.guardianEmail);
    bindText(stmt, 25, a.guardianLanguage);
    bindText(stmt, 26, a.qPreparedness);
    bindText(stmt, 27, a.qGoals);
    bindText(stmt, 28, a.travelExperience);
    bindText(stmt, 29, a.qAdaptability);
    bindText(stmt, 30, a.qTeamwork);
    bindText(stmt, 31, a.qReflection);
    bindText(stmt, 32, a.qFinance);
    sqlite3_bind_int(stmt, 33, a.budget);
    sqlite3_bind_int(stmt, 34, a.prior ? 1 : 0);
    sqlite3_bind_int(stmt, 35, a.declaration ? 1 : 0);
    bindText(stmt, 36, a.status);
    if (a.goodStanding < 0) sqlite3_bind_null(stmt, 37);   // -1 = not checked yet
    else sqlite3_bind_int(stmt, 37, a.goodStanding);
    bindText(stmt, 38, a.submittedAt);
    bindText(stmt, 39, a.createdAt);
    bindText(stmt, 40, a.updatedAt);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

//  documents 

// Reads every uploaded document, once at start-up.
Vector<ApplicationDocument> ApplicationRepo::loadDocuments() {
    Vector<ApplicationDocument> result;

    const char* sql =
        "SELECT student_email, doc_type, file_path, uploaded_at "
        "FROM application_documents";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ApplicationDocument d;
        d.studentEmail = colText(stmt, 0);
        d.docType      = colText(stmt, 1);
        d.filePath     = colText(stmt, 2);
        d.uploadedAt   = colText(stmt, 3);
        result.push_back(d);
    }

    sqlite3_finalize(stmt);
    return result;
}

// Saves one uploaded document. Uploading the same type again replaces the old one.
bool ApplicationRepo::saveDocument(const ApplicationDocument& d) {
    const char* sql =
        "INSERT OR REPLACE INTO application_documents "
        "(student_email, doc_type, file_path, uploaded_at) VALUES (?, ?, ?, ?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    bindText(stmt, 1, d.studentEmail);
    bindText(stmt, 2, d.docType);
    bindText(stmt, 3, d.filePath);
    bindText(stmt, 4, d.uploadedAt);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

// Deletes one uploaded document (when the student removes it before submitting).
bool ApplicationRepo::deleteDocument(const string& email, const string& docType) {
    const char* sql =
        "DELETE FROM application_documents WHERE student_email = ? AND doc_type = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    bindText(stmt, 1, email);
    bindText(stmt, 2, docType);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

// letters

// Reads every letter request, once at start-up.
// The letters service rebuilds each doctor's queue from these (oldest id first).
Vector<LetterRequest> ApplicationRepo::loadLetters() {
    Vector<LetterRequest> result;

    const char* sql =
        "SELECT id, student_email, doctor_name, doctor_email, status, "
        "letter_text, requested_at, answered_at FROM letters";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LetterRequest l;
        l.id           = colInt(stmt, 0, -1);
        l.studentEmail = colText(stmt, 1);
        l.doctorName   = colText(stmt, 2);
        l.doctorEmail  = colText(stmt, 3);
        l.status       = colText(stmt, 4);
        l.letterText   = colText(stmt, 5);
        l.requestedAt  = colText(stmt, 6);
        l.answeredAt   = colText(stmt, 7);
        result.push_back(l);
    }

    sqlite3_finalize(stmt);
    return result;
}

// Adds a new letter request and returns the id the database gave it (-1 if it failed).
int ApplicationRepo::insertLetter(const LetterRequest& l) {
    const char* sql =
        "INSERT INTO letters (student_email, doctor_name, doctor_email, status, requested_at) "
        "VALUES (?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;

    bindText(stmt, 1, l.studentEmail);
    bindText(stmt, 2, l.doctorName);
    bindText(stmt, 3, l.doctorEmail);
    bindText(stmt, 4, l.status);
    bindText(stmt, 5, l.requestedAt);

    int newId = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        newId = static_cast<int>(sqlite3_last_insert_rowid(db));
    }
    sqlite3_finalize(stmt);
    return newId;
}

// Saves the doctor's answer: the new status, the letter text and the date.
bool ApplicationRepo::updateLetter(const LetterRequest& l) {
    const char* sql =
        "UPDATE letters SET status = ?, letter_text = ?, answered_at = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    bindText(stmt, 1, l.status);
    bindText(stmt, 2, l.letterText);
    bindText(stmt, 3, l.answeredAt);
    sqlite3_bind_int(stmt, 4, l.id);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}