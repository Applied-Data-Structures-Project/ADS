// test_recommender.cpp
// Unit tests for RecommenderService (C++ Max-Heap top-K university recommendations).

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "sqlite3.h"
#include "models/student_profile.hpp"
#include "models/university.hpp"
#include "repo/discovery_repo.hpp"
#include "services/discovery_service.hpp"
#include "services/recommender_service.hpp"
#include "test_helpers.hpp"

using namespace std;

static sqlite3* openRecDiscoveryDb() {
    sqlite3* db = nullptr;
    sqlite3_open(":memory:", &db);
    ifstream file(projectRoot() + "/db/migrations/004_discovery.sql");
    stringstream sql;
    sql << file.rdbuf();
    sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, nullptr);
    return db;
}

static University makeUni(const string& slug, const string& name, const string& levels,
                          const string& majors, double minGpa, double livingUsd,
                          const string& tuition = "auc_exchange_waiver",
                          const string& status = "active") {
    University u;
    u.slug = slug;
    u.name = name;
    u.country = "Denmark";
    u.city = "Aarhus";
    u.region = "Europe";
    u.minGpa = minGpa;
    u.studentLevelsJson = levels;
    u.eligibleMajorsJson = majors;
    u.excludedMajorsJson = "[]";
    u.tuitionModel = tuition;
    u.agreementStatus = status;
    u.livingCostUsdEst = livingUsd;
    return u;
}

static StudentRecord makeStudent(const string& major, double gpa, int budget) {
    StudentRecord s;
    s.email = "salma@aucegypt.edu";
    s.level = "UG";
    s.major = major;
    s.gpa = gpa;
    s.hasGpa = true;
    s.credits = 60;
    s.hasCredits = true;
    s.majorDeclared = true;
    s.rhet110 = true;
    s.rhet120 = true;
    s.eligibilitySaved = true;
    s.budget = budget;
    s.hasBudget = budget > 0;
    return s;
}

int main() {
    sqlite3* db = openRecDiscoveryDb();
    DiscoveryRepo discRepo(db);
    DiscoveryService discovery(discRepo);

    Vector<University> catalog;
    catalog.push_back(makeUni("bocconi", "Bocconi University", "[\"UG\", \"GR\"]",
                              "[\"Business\", \"Economics\"]", 3.0, 900.0));
    catalog.push_back(makeUni("aarhus", "Aarhus University", "[\"UG\", \"GR\"]",
                              "[\"ALL\"]", 2.8, 700.0));
    catalog.push_back(makeUni("expensive-u", "Expensive University", "[\"UG\"]",
                              "[\"ALL\"]", 2.8, 2200.0, "direct_fee"));
    catalog.push_back(makeUni("grad-only", "Grad Institute", "[\"GR\"]",
                              "[\"ALL\"]", 3.2, 800.0));
    catalog.push_back(makeUni("high-gpa", "High GPA College", "[\"UG\"]",
                              "[\"ALL\"]", 3.9, 600.0));
    catalog.push_back(makeUni("lund", "Lund University", "[\"UG\", \"GR\"]",
                              "[\"ALL\"]", 2.8, 650.0));

    printf("Top-K Max-Heap ordering & major alignment\n");
    StudentRecord econ = makeStudent("Economics (B.A.)", 3.5, 6000);
    Vector<UniversityRecommendation> topEcon =
        RecommenderService::recommend(econ, catalog, &discovery, 3);
    CHECK(topEcon.size() == 3, "returns exact top 3");
    CHECK(topEcon[0].slug == "bocconi", "specialized Economics partner ranks #1 for Econ student");
    CHECK(topEcon[0].eligible && topEcon[1].eligible && topEcon[2].eligible,
          "all top 3 are eligible");
    CHECK(topEcon[0].fitScore >= topEcon[1].fitScore && topEcon[1].fitScore >= topEcon[2].fitScore,
          "heap pops in non-increasing fit score order");

    printf("Different major filters out restricted partner\n");
    StudentRecord cs = makeStudent("Computer Science (B.S.)", 3.5, 6000);
    Vector<UniversityRecommendation> topCs =
        RecommenderService::recommend(cs, catalog, &discovery, 3);
    CHECK(topCs.size() == 3, "CS student gets top 3");
    CHECK(topCs[0].slug != "bocconi", "Business/Econ-only partner is not #1 for CS student");
    CHECK(topCs[0].eligible && topCs[1].eligible && topCs[2].eligible,
          "eligible schools rank ahead of ineligible Bocconi/grad-only/high-gpa");

    printf("Excludes already-saved universities in O(1)\n");
    string err;
    discovery.saveUniversity("salma@aucegypt.edu", "bocconi", "2026-11-22T20:00:00", err);
    Vector<UniversityRecommendation> afterSave =
        RecommenderService::recommend(econ, catalog, &discovery, 3);
    bool containsBocconi = false;
    for (int i = 0; i < afterSave.size(); i++) {
        if (afterSave[i].slug == "bocconi") containsBocconi = true;
    }
    CHECK(!containsBocconi, "saved university is excluded from recommendations");
    CHECK(afterSave.size() == 3, "still returns 3 unsaved recommendations");

    printf("Budget awareness\n");
    UniversityRecommendation cheapScore =
        RecommenderService::scoreUniversity(cs, catalog[5]); // lund (~$3.5k)
    UniversityRecommendation priceyScore =
        RecommenderService::scoreUniversity(cs, catalog[2]); // expensive-u (direct fee + $2.2k/mo)
    CHECK(cheapScore.withinBudget && !priceyScore.withinBudget,
          "withinBudget accurately reflects semester cost vs student budget");
    CHECK(cheapScore.fitScore > priceyScore.fitScore,
          "in-budget exchange waiver school outscores expensive direct-fee school");

    printf("Saved sorting (C++ Min-Heap & mergeSort)\n");
    Vector<University> savedSubset;
    savedSubset.push_back(catalog[0]); // bocconi
    savedSubset.push_back(catalog[1]); // aarhus
    savedSubset.push_back(catalog[2]); // expensive-u
    savedSubset.push_back(catalog[5]); // lund

    Vector<UniversityRecommendation> byDeadline =
        RecommenderService::sortSaved(econ, savedSubset, "deadline");
    CHECK(byDeadline.size() == 4 &&
              byDeadline[0].deadline <= byDeadline[1].deadline &&
              byDeadline[1].deadline <= byDeadline[2].deadline &&
              byDeadline[2].deadline <= byDeadline[3].deadline,
          "sortSaved(deadline) orders nearest deadline first via C++ Min-Heap");

    Vector<UniversityRecommendation> byFit =
        RecommenderService::sortSaved(econ, savedSubset, "fit");
    CHECK(byFit.size() == 4 &&
              byFit[0].fitScore >= byFit[1].fitScore &&
              byFit[1].fitScore >= byFit[2].fitScore &&
              byFit[2].fitScore >= byFit[3].fitScore,
          "sortSaved(fit) orders highest fit first via C++ mergeSort");

    Vector<UniversityRecommendation> byCost =
        RecommenderService::sortSaved(econ, savedSubset, "cost");
    CHECK(byCost.size() == 4 &&
              byCost[0].estimatedSemesterCostUsd <= byCost[1].estimatedSemesterCostUsd &&
              byCost[1].estimatedSemesterCostUsd <= byCost[2].estimatedSemesterCostUsd &&
              byCost[2].estimatedSemesterCostUsd <= byCost[3].estimatedSemesterCostUsd,
          "sortSaved(cost) orders lowest semester cost first via C++ mergeSort");

    printf("Edge cases\n");
    Vector<University> emptyCatalog;
    CHECK(RecommenderService::recommend(econ, emptyCatalog, &discovery, 3).size() == 0,
          "empty catalog returns 0 items");
    CHECK(RecommenderService::recommend(econ, catalog, &discovery, 0).size() == 0,
          "k=0 returns 0 items");

    sqlite3_close(db);
    if (failures > 0) {
        printf("SOME FAILED (%d failed)\n", failures);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
