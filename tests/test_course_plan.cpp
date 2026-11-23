// test_course_plan.cpp
// Tests CoursePlanService + CoursePlanRepo (HashMap, Vector, mergeSort, SQLite persistence).

#include <iostream>
#include <string>

#include "test_helpers.hpp"
#include "repo/course_plan_repo.hpp"
#include "services/course_plan_service.hpp"

using namespace std;

static void check(bool condition, const string& label) {
    if (!condition) {
        cerr << "FAILED: " << label << "\n";
        exit(1);
    }
}

int main() {
    sqlite3* db = openTestDb();
    {
        ifstream migration(projectRoot() + "/db/migrations/006_course_plans.sql");
        stringstream sql;
        sql << migration.rdbuf();
        char* errMsg = nullptr;
        check(sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, &errMsg) == SQLITE_OK,
              string("run 006_course_plans.sql: ") + (errMsg ? errMsg : ""));
        sqlite3_free(errMsg);
    }

    CoursePlanRepo repo(db);
    CoursePlanService svc(repo);
    svc.loadAll();

    // 1. Save a plan for Aarhus with one empty row that should be stripped.
    CoursePlan p1;
    p1.studentEmail = "Salma@aucegypt.edu";
    p1.universitySlug = "aarhus-university";
    p1.universityName = "Aarhus University";
    p1.bannerInstCode = "201842";
    p1.bannerInstName = "Aarhus University";
    p1.studyTerm = "Fall 2027";

    PlannedCourse c1;
    c1.hostCourseCode = "ALG BIO";
    c1.hostCourseTitle = "Algorithms in Bioinformatics";
    c1.aucEquivalent = "BIOT 5206 Fundamentals of Bioinformatics, cr.hrs.: 3";
    c1.effectiveTerm = "Fall 2023";
    c1.source = "banner";
    p1.courses.push_back(c1);

    PlannedCourse blank;
    p1.courses.push_back(blank);

    PlannedCourse c2;
    c2.hostCourseCode = "CS 301";
    c2.hostCourseTitle = "Distributed Systems";
    c2.aucEquivalent = "Pending advisor review";
    c2.source = "manual";
    p1.courses.push_back(c2);

    string err;
    check(svc.savePlan(p1, "2026-11-23T10:00:00", err), "save first course plan: " + err);

    const CoursePlan* found = svc.find("salma@aucegypt.edu", "aarhus-university");
    check(found != nullptr, "case-insensitive email lookup works");
    check(found->courses.size() == 2, "blank course slot stripped");
    check(found->courses[0].hostCourseCode == "ALG BIO", "first course code preserved");
    check(found->courses[0].source == "banner", "banner source preserved");
    check(found->courses[1].source == "manual", "manual source preserved");

    // 2. Save a second, newer plan for Bocconi and verify mergeSort orders newest first.
    CoursePlan p2;
    p2.studentEmail = "salma@aucegypt.edu";
    p2.universitySlug = "bocconi-university";
    p2.universityName = "Bocconi University";
    p2.studyTerm = "Spring 2028";
    PlannedCourse c3;
    c3.hostCourseCode = "30201";
    c3.hostCourseTitle = "International Macroeconomics";
    c3.aucEquivalent = "ECON 3021";
    c3.source = "banner";
    p2.courses.push_back(c3);

    check(svc.savePlan(p2, "2026-11-23T12:30:00", err), "save second plan");
    Vector<CoursePlan> list = svc.plansOf("salma@aucegypt.edu");
    check(list.size() == 2, "student has 2 plans");
    check(list[0].universitySlug == "bocconi-university", "newest plan sorted first via mergeSort");
    check(list[1].universitySlug == "aarhus-university", "older plan sorted second");

    // 3. Enforce MAX_COURSES_PER_PLAN limit.
    CoursePlan tooMany = p2;
    for (int i = 0; i < CoursePlanService::MAX_COURSES_PER_PLAN + 1; i++) {
        PlannedCourse extra;
        extra.hostCourseCode = "C" + to_string(i);
        extra.hostCourseTitle = "Course " + to_string(i);
        tooMany.courses.push_back(extra);
    }
    check(!svc.savePlan(tooMany, "2026-11-23T13:00:00", err), "rejects >15 courses");

    // 4. Reload into a fresh CoursePlanService instance to verify SQLite persistence.
    CoursePlanService reloaded(repo);
    reloaded.loadAll();
    Vector<CoursePlan> reloadedList = reloaded.plansOf("salma@aucegypt.edu");
    check(reloadedList.size() == 2, "reloaded 2 plans from SQLite");
    check(reloadedList[0].universitySlug == "bocconi-university", "reloaded newest first");
    check(reloadedList[1].courses.size() == 2, "reloaded Aarhus plan items intact");

    // 5. Delete plan and verify removal both in memory and in SQLite.
    check(reloaded.deletePlan("salma@aucegypt.edu", "bocconi-university", err), "delete Bocconi plan");
    check(reloaded.find("salma@aucegypt.edu", "bocconi-university") == nullptr, "deleted plan gone from HashMap");
    check(reloaded.plansOf("salma@aucegypt.edu").size() == 1, "1 plan remains");

    sqlite3_close(db);
    cout << "All course plan tests passed.\n";
    return 0;
}
