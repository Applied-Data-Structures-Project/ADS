// student_repo.cpp
// Loads and saves rows in the `students` table.

#include "repo/student_repo.hpp"

using namespace std;

static string colText(sqlite3_stmt* s, int i) {
    const unsigned char* t = sqlite3_column_text(s, i);
    return t ? reinterpret_cast<const char*>(t) : "";
}

static int colInt(sqlite3_stmt* s, int i, int ifNull = 0) {
    if (sqlite3_column_type(s, i) == SQLITE_NULL) return ifNull;
    return sqlite3_column_int(s, i);
}

static double colDouble(sqlite3_stmt* s, int i) {
    if (sqlite3_column_type(s, i) == SQLITE_NULL) return 0.0;
    return sqlite3_column_double(s, i);
}

static void bindText(sqlite3_stmt* s, int i, const string& value) {
    if (value.empty()) sqlite3_bind_null(s, i);
    else sqlite3_bind_text(s, i, value.c_str(), -1, SQLITE_TRANSIENT);
}

StudentRepo::StudentRepo(sqlite3* db) : db(db) {}

Vector<StudentRecord> StudentRepo::loadStudents() {
    Vector<StudentRecord> result;

    const char* sql =
        "SELECT email, name, auc_id, phone, level, major, "
        "gpa, has_gpa, credits, has_credits, "
        "major_declared, rhet110, rhet120, final_semester, "
        "academic_probation, disciplinary_probation, "
        "english_complete, first_semester_no_gpa, eligibility_saved, "
        "budget, has_budget, updated_at "
        "FROM students";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StudentRecord r;
        r.email                 = colText(stmt, 0);
        r.name                  = colText(stmt, 1);
        r.aucId                 = colText(stmt, 2);
        r.phone                 = colText(stmt, 3);
        r.level                 = colText(stmt, 4);
        r.major                 = colText(stmt, 5);
        r.gpa                   = colDouble(stmt, 6);
        r.hasGpa                = colInt(stmt, 7) == 1;
        r.credits               = colInt(stmt, 8);
        r.hasCredits            = colInt(stmt, 9) == 1;
        r.majorDeclared         = colInt(stmt, 10) == 1;
        r.rhet110               = colInt(stmt, 11) == 1;
        r.rhet120               = colInt(stmt, 12) == 1;
        r.finalSemester         = colInt(stmt, 13) == 1;
        r.academicProbation     = colInt(stmt, 14) == 1;
        r.disciplinaryProbation = colInt(stmt, 15) == 1;
        r.englishComplete       = colInt(stmt, 16) == 1;
        r.firstSemesterNoGpa    = colInt(stmt, 17) == 1;
        r.eligibilitySaved      = colInt(stmt, 18) == 1;
        r.budget                = colInt(stmt, 19);
        r.hasBudget             = colInt(stmt, 20) == 1;
        r.updatedAt             = colText(stmt, 21);
        result.push_back(r);
    }

    sqlite3_finalize(stmt);
    return result;
}

bool StudentRepo::saveStudent(const StudentRecord& r) {
    const char* sql =
        "INSERT OR REPLACE INTO students ("
        "email, name, auc_id, phone, level, major, "
        "gpa, has_gpa, credits, has_credits, "
        "major_declared, rhet110, rhet120, final_semester, "
        "academic_probation, disciplinary_probation, "
        "english_complete, first_semester_no_gpa, eligibility_saved, "
        "budget, has_budget, updated_at"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    bindText(stmt, 1, r.email);
    bindText(stmt, 2, r.name);
    bindText(stmt, 3, r.aucId);
    bindText(stmt, 4, r.phone);
    bindText(stmt, 5, r.level);
    bindText(stmt, 6, r.major);
    if (r.hasGpa) sqlite3_bind_double(stmt, 7, r.gpa);
    else sqlite3_bind_null(stmt, 7);
    sqlite3_bind_int(stmt, 8, r.hasGpa ? 1 : 0);
    if (r.hasCredits) sqlite3_bind_int(stmt, 9, r.credits);
    else sqlite3_bind_null(stmt, 9);
    sqlite3_bind_int(stmt, 10, r.hasCredits ? 1 : 0);
    sqlite3_bind_int(stmt, 11, r.majorDeclared ? 1 : 0);
    sqlite3_bind_int(stmt, 12, r.rhet110 ? 1 : 0);
    sqlite3_bind_int(stmt, 13, r.rhet120 ? 1 : 0);
    sqlite3_bind_int(stmt, 14, r.finalSemester ? 1 : 0);
    sqlite3_bind_int(stmt, 15, r.academicProbation ? 1 : 0);
    sqlite3_bind_int(stmt, 16, r.disciplinaryProbation ? 1 : 0);
    sqlite3_bind_int(stmt, 17, r.englishComplete ? 1 : 0);
    sqlite3_bind_int(stmt, 18, r.firstSemesterNoGpa ? 1 : 0);
    sqlite3_bind_int(stmt, 19, r.eligibilitySaved ? 1 : 0);
    sqlite3_bind_int(stmt, 20, r.budget);
    sqlite3_bind_int(stmt, 21, r.hasBudget ? 1 : 0);
    bindText(stmt, 22, r.updatedAt);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}
