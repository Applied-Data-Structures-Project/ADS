// course_plan_repo.cpp
// Loads and saves rows in `course_plans` and `course_plan_items`.

#include "repo/course_plan_repo.hpp"
#include "hashmap.hpp"

using namespace std;

static string colText(sqlite3_stmt* s, int i) {
    const unsigned char* t = sqlite3_column_text(s, i);
    return t ? reinterpret_cast<const char*>(t) : "";
}

static string planKey(const string& email, const string& slug) {
    return email + "|" + slug;
}

CoursePlanRepo::CoursePlanRepo(sqlite3* db) : db(db) {}

Vector<CoursePlan> CoursePlanRepo::loadAll() {
    Vector<CoursePlan> plans;
    HashMap<string, int> indexByKey;

    const char* planSql =
        "SELECT student_email, university_slug, university_name, "
        "       banner_inst_code, banner_inst_name, study_term, updated_at "
        "FROM course_plans "
        "ORDER BY updated_at DESC, rowid ASC";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, planSql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            CoursePlan p;
            p.studentEmail = colText(stmt, 0);
            p.universitySlug = colText(stmt, 1);
            p.universityName = colText(stmt, 2);
            p.bannerInstCode = colText(stmt, 3);
            p.bannerInstName = colText(stmt, 4);
            p.studyTerm = colText(stmt, 5);
            p.updatedAt = colText(stmt, 6);
            int idx = plans.size();
            plans.push_back(p);
            indexByKey.insert(planKey(p.studentEmail, p.universitySlug), idx);
        }
        sqlite3_finalize(stmt);
    }

    const char* itemSql =
        "SELECT student_email, university_slug, host_course_code, host_course_title, "
        "       auc_equivalent, effective_term, source "
        "FROM course_plan_items "
        "ORDER BY position ASC, id ASC";

    stmt = nullptr;
    if (sqlite3_prepare_v2(db, itemSql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            string email = colText(stmt, 0);
            string slug = colText(stmt, 1);
            int* idx = indexByKey.get(planKey(email, slug));
            if (!idx) continue;

            PlannedCourse c;
            c.hostCourseCode = colText(stmt, 2);
            c.hostCourseTitle = colText(stmt, 3);
            c.aucEquivalent = colText(stmt, 4);
            c.effectiveTerm = colText(stmt, 5);
            c.source = colText(stmt, 6);
            plans[*idx].courses.push_back(c);
        }
        sqlite3_finalize(stmt);
    }

    return plans;
}

bool CoursePlanRepo::upsertPlan(const CoursePlan& plan) {
    char* err = nullptr;
    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }

    bool ok = true;

    const char* upsertSql =
        "INSERT INTO course_plans "
        "  (student_email, university_slug, university_name, banner_inst_code, banner_inst_name, study_term, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(student_email, university_slug) DO UPDATE SET "
        "  university_name = excluded.university_name, "
        "  banner_inst_code = excluded.banner_inst_code, "
        "  banner_inst_name = excluded.banner_inst_name, "
        "  study_term = excluded.study_term, "
        "  updated_at = excluded.updated_at";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, upsertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        ok = false;
    } else {
        sqlite3_bind_text(stmt, 1, plan.studentEmail.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, plan.universitySlug.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, plan.universityName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, plan.bannerInstCode.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, plan.bannerInstName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, plan.studyTerm.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, plan.updatedAt.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }

    if (ok) {
        const char* clearSql =
            "DELETE FROM course_plan_items WHERE student_email = ? AND university_slug = ?";
        stmt = nullptr;
        if (sqlite3_prepare_v2(db, clearSql, -1, &stmt, nullptr) != SQLITE_OK) {
            ok = false;
        } else {
            sqlite3_bind_text(stmt, 1, plan.studentEmail.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, plan.universitySlug.c_str(), -1, SQLITE_TRANSIENT);
            ok = (sqlite3_step(stmt) == SQLITE_DONE);
            sqlite3_finalize(stmt);
        }
    }

    if (ok && plan.courses.size() > 0) {
        const char* insertSql =
            "INSERT INTO course_plan_items "
            "  (student_email, university_slug, position, host_course_code, host_course_title, "
            "   auc_equivalent, effective_term, source) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        stmt = nullptr;
        if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
            ok = false;
        } else {
            for (int i = 0; i < plan.courses.size(); i++) {
                const PlannedCourse& c = plan.courses[i];
                sqlite3_reset(stmt);
                sqlite3_clear_bindings(stmt);
                sqlite3_bind_text(stmt, 1, plan.studentEmail.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, plan.universitySlug.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt, 3, i);
                sqlite3_bind_text(stmt, 4, c.hostCourseCode.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 5, c.hostCourseTitle.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 6, c.aucEquivalent.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 7, c.effectiveTerm.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 8, c.source.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    ok = false;
                    break;
                }
            }
            sqlite3_finalize(stmt);
        }
    }

    if (ok) {
        if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK) {
            sqlite3_free(err);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
        return true;
    }

    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return false;
}

bool CoursePlanRepo::deletePlan(const string& studentEmail, const string& universitySlug) {
    char* err = nullptr;
    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }

    bool ok = true;
    sqlite3_stmt* stmt = nullptr;

    const char* delItems =
        "DELETE FROM course_plan_items WHERE student_email = ? AND university_slug = ?";
    if (sqlite3_prepare_v2(db, delItems, -1, &stmt, nullptr) != SQLITE_OK) {
        ok = false;
    } else {
        sqlite3_bind_text(stmt, 1, studentEmail.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, universitySlug.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }

    if (ok) {
        const char* delPlan =
            "DELETE FROM course_plans WHERE student_email = ? AND university_slug = ?";
        stmt = nullptr;
        if (sqlite3_prepare_v2(db, delPlan, -1, &stmt, nullptr) != SQLITE_OK) {
            ok = false;
        } else {
            sqlite3_bind_text(stmt, 1, studentEmail.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, universitySlug.c_str(), -1, SQLITE_TRANSIENT);
            ok = (sqlite3_step(stmt) == SQLITE_DONE);
            sqlite3_finalize(stmt);
        }
    }

    if (ok) {
        if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK) {
            sqlite3_free(err);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
        return true;
    }

    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    return false;
}
