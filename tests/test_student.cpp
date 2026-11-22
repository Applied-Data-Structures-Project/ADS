// test_student.cpp
// Unit tests for StudentRepo and StudentService (profile, eligibility, budget, persistence).

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "sqlite3.h"
#include "repo/student_repo.hpp"
#include "services/student_service.hpp"
#include "test_helpers.hpp"

using namespace std;

static sqlite3* openStudentTestDb() {
    sqlite3* db = nullptr;
    sqlite3_open(":memory:", &db);
    ifstream file(projectRoot() + "/db/migrations/003_students.sql");
    stringstream sql;
    sql << file.rdbuf();
    if (sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
        printf("  Could not create students table: %s\n", sqlite3_errmsg(db));
        failures++;
    }
    return db;
}

static StudentRecord eligibleUG() {
    StudentRecord r;
    r.email = "salma@aucegypt.edu";
    r.name = "Salma Hassan";
    r.aucId = "900214512";
    r.phone = "+201001234567";
    r.level = "UG";
    r.major = "Computer Science";
    r.gpa = 3.5;
    r.hasGpa = true;
    r.credits = 60;
    r.hasCredits = true;
    r.majorDeclared = true;
    r.rhet110 = true;
    r.rhet120 = true;
    r.finalSemester = false;
    r.academicProbation = false;
    r.disciplinaryProbation = false;
    r.eligibilitySaved = true;
    r.budget = 5000;
    r.hasBudget = true;
    return r;
}

int main() {
    sqlite3* db = openStudentTestDb();
    StudentRepo repo(db);
    StudentService svc(repo);
    string err;

    printf("Saving and lookup\n");
    CHECK(svc.find("salma@aucegypt.edu") == nullptr, "unknown student returns nullptr");

    StudentRecord salma = eligibleUG();
    salma.email = "Salma@AUCEgypt.edu";
    CHECK(svc.save(salma, "2026-11-22T19:00:00", err), "valid UG profile saves");

    StudentRecord* found = svc.find("salma@aucegypt.edu");
    CHECK(found != nullptr, "student is found in HashMap by lowercase email");
    CHECK(found != nullptr && found->major == "Computer Science", "major stored");
    CHECK(found != nullptr && found->budget == 5000 && found->hasBudget, "budget stored");
    CHECK(found != nullptr && found->updatedAt == "2026-11-22T19:00:00", "updatedAt stamped");

    printf("Validation\n");
    StudentRecord bad = eligibleUG();
    bad.email = "salma@gmail.com";
    CHECK(!svc.save(bad, "2026-11-22T19:01:00", err), "non-AUC email rejected");

    bad = eligibleUG();
    bad.gpa = 4.5;
    CHECK(!svc.save(bad, "2026-11-22T19:01:00", err), "GPA > 4.0 rejected");

    bad = eligibleUG();
    bad.credits = 350;
    CHECK(!svc.save(bad, "2026-11-22T19:01:00", err), "credits > 300 rejected");

    bad = eligibleUG();
    bad.major = "A";
    CHECK(!svc.save(bad, "2026-11-22T19:01:00", err), "short major rejected when eligibilitySaved");

    StudentRecord budgetOnly;
    budgetOnly.email = "omar@aucegypt.edu";
    budgetOnly.budget = 4200;
    budgetOnly.hasBudget = true;
    budgetOnly.eligibilitySaved = false;
    CHECK(svc.save(budgetOnly, "2026-11-22T19:02:00", err), "partial budget-only record saves before /profile");

    printf("Eligibility evaluation\n");
    StudentEligibilityOutcome out = StudentService::evaluateEligibility(*found);
    CHECK(out.eligible && !out.provisional, "complete UG profile is eligible");
    CHECK(StudentService::checkEligibilityReason(*found).empty(), "eligible UG has empty reason");

    StudentRecord lowGpa = eligibleUG();
    lowGpa.gpa = 2.5;
    CHECK(!StudentService::evaluateEligibility(lowGpa).eligible, "UG GPA 2.5 is ineligible");
    CHECK(!StudentService::checkEligibilityReason(lowGpa).empty(), "UG low GPA gives reason");

    StudentRecord probation = eligibleUG();
    probation.academicProbation = true;
    CHECK(!StudentService::evaluateEligibility(probation).eligible, "academic probation is ineligible");

    StudentRecord discProb = eligibleUG();
    discProb.disciplinaryProbation = true;
    CHECK(!StudentService::evaluateEligibility(discProb).eligible, "disciplinary probation is ineligible");

    StudentRecord noRhet = eligibleUG();
    noRhet.rhet120 = false;
    CHECK(!StudentService::evaluateEligibility(noRhet).eligible, "missing RHET 120 is ineligible");

    StudentRecord finalSem = eligibleUG();
    finalSem.finalSemester = true;
    CHECK(!StudentService::evaluateEligibility(finalSem).eligible, "UG final semester is ineligible");

    StudentRecord grFirstSem;
    grFirstSem.email = "nadia@aucegypt.edu";
    grFirstSem.level = "GR";
    grFirstSem.major = "Economics";
    grFirstSem.hasGpa = false;
    grFirstSem.firstSemesterNoGpa = true;
    grFirstSem.hasCredits = true;
    grFirstSem.credits = 0;
    grFirstSem.majorDeclared = false;
    grFirstSem.englishComplete = true;
    grFirstSem.eligibilitySaved = true;
    CHECK(svc.save(grFirstSem, "2026-11-22T19:03:00", err), "GR first-semester without GPA saves");
    StudentEligibilityOutcome grOut = StudentService::evaluateEligibility(grFirstSem);
    CHECK(grOut.eligible && grOut.provisional, "GR first-semester without GPA is provisionally eligible");

    StudentRecord grLowCredits = grFirstSem;
    grLowCredits.firstSemesterNoGpa = false;
    grLowCredits.hasGpa = true;
    grLowCredits.gpa = 3.5;
    grLowCredits.credits = 12;
    CHECK(StudentService::evaluateEligibility(grLowCredits).eligible, "GR with 12 credits (<24) is eligible");

    StudentRecord grNoEnglish = grLowCredits;
    grNoEnglish.englishComplete = false;
    CHECK(!StudentService::evaluateEligibility(grNoEnglish).eligible, "GR missing English is ineligible");

    printf("Restart reload (loadAll)\n");
    StudentService fresh(repo);
    fresh.loadAll();
    StudentRecord* reloaded = fresh.find("salma@aucegypt.edu");
    CHECK(reloaded != nullptr, "Salma reloads after restart");
    CHECK(reloaded != nullptr && reloaded->hasGpa && reloaded->gpa == 3.5, "GPA survives restart");
    CHECK(reloaded != nullptr && reloaded->rhet110 && reloaded->rhet120, "RHET flags survive restart");
    CHECK(reloaded != nullptr && reloaded->budget == 5000 && reloaded->hasBudget, "budget survives restart");

    StudentRecord* reloadedGr = fresh.find("nadia@aucegypt.edu");
    CHECK(reloadedGr != nullptr && !reloadedGr->hasGpa && reloadedGr->firstSemesterNoGpa,
          "GR null GPA survives restart");

    sqlite3_close(db);
    if (failures > 0) {
        printf("SOME FAILED (%d failed)\n", failures);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
