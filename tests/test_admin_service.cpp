// Standalone test & 500-applicant stress test for AdminRepo + AdminService.
// Runs against an in-memory SQLite database (:memory:) and verifies:
//   - Partner catalog loading, filtering, and persistent seat quota overrides
//   - Real application audit, statement rating, status updates, and student notifications
//   - 500-applicant realistic demo cohort generation + all 4 AdminAllocator modes
//     (best_fit, highest_score, most_placed, shortlists) + diffRuns + publishRun + clean clear

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "sqlite3.h"
#include "repo/admin_repo.hpp"
#include "repo/application_repo.hpp"
#include "repo/discovery_repo.hpp"
#include "services/admin_service.hpp"
#include "services/application_service.hpp"
#include "services/discovery_service.hpp"

namespace
{

int failures = 0;

#define CHECK(condition, name)                                      \
    do {                                                            \
        if (condition) {                                            \
            std::printf("  PASS  %s\n", name);                      \
        } else {                                                    \
            std::printf("  FAIL  %s  (line %d)\n", name, __LINE__); \
            failures++;                                             \
        }                                                           \
    } while (0)

std::string projectRoot()
{
    std::string here = __FILE__;
    std::size_t pos = here.rfind("tests/");
    if (pos == std::string::npos)
        pos = here.rfind("tests\\");
    if (pos == std::string::npos || pos == 0)
        return ".";
    return here.substr(0, pos - 1);
}

bool runSqlFile(sqlite3 *db, const std::string &path)
{
    std::ifstream in(path);
    if (!in)
        return false;
    std::stringstream sql;
    sql << in.rdbuf();
    char *err = nullptr;
    bool ok = (sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, &err) == SQLITE_OK);
    if (!ok)
    {
        std::printf("SQL error in %s: %s\n", path.c_str(), err ? err : "?");
        sqlite3_free(err);
    }
    return ok;
}

sqlite3 *openAdminTestDb()
{
    sqlite3 *db = nullptr;
    sqlite3_open(":memory:", &db);

    const char *uniSchema =
        "CREATE TABLE universities ("
        "slug TEXT PRIMARY KEY,"
        "excel_rows_json TEXT NOT NULL DEFAULT '[]',"
        "shard INTEGER NOT NULL DEFAULT 1,"
        "fact_sheet_match_status TEXT NOT NULL DEFAULT 'matched',"
        "fact_sheet_files_json TEXT NOT NULL DEFAULT '[]',"
        "cross_references_json TEXT NOT NULL DEFAULT '[]',"
        "official_name TEXT, short_name TEXT, country TEXT, state_or_province TEXT, city TEXT, region TEXT,"
        "agreement_status TEXT, agreement_student_levels_json TEXT, min_standing_note TEXT, agreement_scope_raw TEXT,"
        "eligible_auc_schools_json TEXT, eligible_majors_json TEXT, excluded_majors_json TEXT, min_gpa_host REAL,"
        "instruction_languages_json TEXT, english_course_availability TEXT, english_proficiency_and_rhet1020 TEXT,"
        "host_language_requirement TEXT, available_terms_json TEXT, fall_term_dates TEXT, spring_term_dates TEXT,"
        "final_semester_allowed INTEGER, seat_quota INTEGER, tuition_model TEXT, est_monthly_living_currency TEXT,"
        "est_monthly_living_min REAL, est_monthly_living_max REAL, living_cost_usd_est REAL,"
        "living_cost_usd_est_status TEXT NOT NULL DEFAULT 'partial', mandatory_host_fees_and_insurance TEXT,"
        "housing_availability TEXT, housing_notes TEXT, host_grants_or_stipends TEXT, citizenship_restrictions TEXT,"
        "visa_lead_time_and_financial_proof TEXT, official_website_url TEXT, exchange_page_url TEXT,"
        "course_catalog_url TEXT, english_course_list_url TEXT, credit_system_and_load TEXT,"
        "course_restrictions_note TEXT, required_application_docs_json TEXT, academic_style TEXT,"
        "city_tier TEXT, campus_type TEXT, daily_life_english_friendliness TEXT,"
        "international_support_services TEXT, nomination_deadlines TEXT,"
        "ext_json TEXT NOT NULL DEFAULT '{}', metadata_json TEXT NOT NULL DEFAULT '{}'"
        ");"
        "CREATE TABLE students (email TEXT PRIMARY KEY);";

    sqlite3_exec(db, uniSchema, nullptr, nullptr, nullptr);
    sqlite3_exec(db, "DROP TABLE students;", nullptr, nullptr, nullptr);

    std::string root = projectRoot();
    CHECK(runSqlFile(db, root + "/db/migrations/002_application.sql"), "apply 002_application.sql");
    CHECK(runSqlFile(db, root + "/db/migrations/003_students.sql"), "apply 003_students.sql");
    CHECK(runSqlFile(db, root + "/db/migrations/004_discovery.sql"), "apply 004_discovery.sql");
    CHECK(runSqlFile(db, root + "/db/migrations/005_admin.sql"), "apply 005_admin.sql");

    // Seed 155 realistic universities matching our flagship slugs + broad pool
    const char *flagships[10][6] = {
        {"bocconi-university", "Bocconi University", "Italy", "Europe", "[\"Business\",\"Economics\"]", "[\"Fall\",\"Spring\"]"},
        {"aarhus-university", "Aarhus University", "Denmark", "Europe", "[\"ALL\"]", "[\"Spring\",\"Academic Year\",\"Summer\"]"},
        {"technical-university-of-munich-tum", "Technical University of Munich (TUM)", "Germany", "Europe", "[\"Engineering\",\"Business\"]", "[\"Fall\",\"Spring\"]"},
        {"sciences-po-institute-d-etudes-politiques-de-paris", "Sciences Po", "France", "Europe", "[\"Political Science\",\"Economics\"]", "[\"Fall\",\"Spring\"]"},
        {"vrije-universiteit-amsterdam", "Vrije Universiteit Amsterdam", "Netherlands", "Europe", "[\"ALL\"]", "[\"Fall\",\"Spring\"]"},
        {"university-of-edinburgh", "University of Edinburgh", "United Kingdom", "Europe", "[\"ALL\"]", "[\"Fall\",\"Spring\"]"},
        {"king-s-college-london-kcl", "King''s College London", "United Kingdom", "Europe", "[\"ALL\"]", "[\"Fall\",\"Spring\"]"},
        {"copenhagen-business-school", "Copenhagen Business School", "Denmark", "Europe", "[\"Business\",\"Economics\"]", "[\"Fall\",\"Spring\"]"},
        {"lund-university", "Lund University", "Sweden", "Europe", "[\"ALL\"]", "[\"Fall\",\"Spring\"]"},
        {"akita-international-university", "Akita International University", "Japan", "East Asia", "[\"ALL\"]", "[\"Fall\",\"Spring\"]"}
    };

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    for (int i = 0; i < 155; ++i)
    {
        char sqlBuf[1024];
        if (i < 10)
        {
            std::snprintf(sqlBuf, sizeof(sqlBuf),
                          "INSERT INTO universities (slug, official_name, short_name, country, city, region, "
                          "agreement_status, agreement_student_levels_json, eligible_majors_json, available_terms_json, "
                          "min_gpa_host, living_cost_usd_est, tuition_model, housing_availability, fact_sheet_match_status) "
                          "VALUES ('%s','%s','U%d','%s','City','%s','active','[\"UG\",\"GR\"]','%s','%s', %s, 5200, 'auc_exchange_waiver', 'assisted', 'matched');",
                          flagships[i][0], flagships[i][1], i, flagships[i][2], flagships[i][3],
                          flagships[i][4], flagships[i][5], (i == 0 || i == 9) ? "3.0" : "NULL");
        }
        else
        {
            const char *region = (i % 3 == 0) ? "North America" : ((i % 3 == 1) ? "Europe" : "East Asia");
            std::snprintf(sqlBuf, sizeof(sqlBuf),
                          "INSERT INTO universities (slug, official_name, short_name, country, city, region, "
                          "agreement_status, agreement_student_levels_json, eligible_majors_json, available_terms_json, "
                          "min_gpa_host, living_cost_usd_est, tuition_model, housing_availability, fact_sheet_match_status) "
                          "VALUES ('partner-%03d','Partner University %03d','P%03d','Country','City','%s',"
                          "'active','[\"UG\"]','[\"ALL\"]','[\"Fall\",\"Spring\"]', 2.9, 4800, 'auc_exchange_waiver', 'guaranteed', 'matched');",
                          i, i, i, region);
        }
        sqlite3_exec(db, sqlBuf, nullptr, nullptr, nullptr);
    }
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

    return db;
}

} // namespace

int main()
{
    std::printf("Running test_admin_service...\n");
    sqlite3 *db = openAdminTestDb();

    ApplicationRepo appRepo(db);
    DiscoveryRepo discRepo(db);
    AdminRepo adminRepo(db);

    ApplicationService appService(appRepo);
    DiscoveryService discService(discRepo);
    appService.loadAll();
    discService.loadAll();

    // Insert 1 real non-demo submitted student before testing demo seeding/cleanup
    sqlite3_exec(db,
                 "INSERT INTO students (email, name, auc_id, level, major, gpa, has_gpa, credits, has_credits, "
                 "major_declared, rhet110, rhet120, eligibility_saved, budget, has_budget) "
                 "VALUES ('real.student@aucegypt.edu','Nour Ali','900999001','UG','Computer Science (CSCE)',"
                 "3.88,1,75,1,1,1,1,1,6000,1);",
                 nullptr, nullptr, nullptr);
    sqlite3_exec(db,
                 "INSERT INTO applications (student_email, first_name, surname, auc_id, gpa, level, major, "
                 "pref1, pref2, pref3, status, prior, budget) "
                 "VALUES ('real.student@aucegypt.edu','Nour','Ali','900999001',3.88,'UG','Computer Science (CSCE)',"
                 "'aarhus-university','vrije-universiteit-amsterdam','lund-university','Submitted',0,6000);",
                 nullptr, nullptr, nullptr);

    AdminService admin(adminRepo, appService, discService);
    admin.loadAll();

    // 1. Partner listing & seat override persistence
    AdminPartnerStats pStats;
    AdminPartnerFilter pf;
    Vector<AdminPartnerDetail> partners = admin.listPartners(pf, pStats);
    CHECK(partners.size() == 155, "loads all 155 partner universities");
    CHECK(pStats.totalConfiguredSeats == 310, "default seats 2 * 155 = 310");

    AdminPartnerDetail updatedPartner;
    std::string err;
    CHECK(admin.setPartnerSeats("bocconi-university", 6, "2026-11-22T12:00:00", updatedPartner, err),
          "persists custom seat override");
    CHECK(updatedPartner.seats == 6 && !updatedPartner.seatsAssumed,
          "custom seat count reflected and seatsAssumed = false");

    // Verify term filtering recognizes "Academic Year" as offering Fall & Spring (e.g. Aarhus)
    AdminPartnerFilter fallFilter;
    fallFilter.search = "Aarhus";
    fallFilter.term = "Fall";
    Vector<AdminPartnerDetail> fallAarhus = admin.listPartners(fallFilter, pStats);
    CHECK(fallAarhus.size() == 1, "Academic Year university matches Fall term filter");

    // 2. Real application review + rating + notification
    AdminReviewUpdate revUpd;
    revUpd.status = "Verified";
    revUpd.hasGoodStanding = true;
    revUpd.goodStanding = 1;
    revUpd.updateStatementRating = true;
    revUpd.hasStatementRating = true;
    revUpd.statementRating = 9.1;
    revUpd.updateNotes = true;
    revUpd.notes = "Strong CS transcript and essay.";
    AdminApplicationRow updatedApp;
    CHECK(admin.updateApplicationReview("real.student@aucegypt.edu", revUpd, "2026-11-22T12:05:00", updatedApp, err),
          "updates real student review, status, and statement rating");
    CHECK(updatedApp.status == "Verified" && updatedApp.hasStatementRating && updatedApp.statementRating > 9.0,
          "updated review fields returned on dossier");
    CHECK(discService.unreadCount("real.student@aucegypt.edu") == 1,
          "verification notifies real student");

    // 3. Seed 500-applicant demo stress cohort
    int seeded = 0;
    CHECK(admin.seedDemoCohort("2026-11-22T12:10:00", seeded, err), "seeds 500 demo applicants");
    CHECK(seeded == 500, "seeded count is 500");

    AdminOverviewStats overview = admin.getOverviewStats("2026–27 · Fall Term");
    CHECK(overview.totalApplicants == 501, "overview sees 500 demo + 1 real applicant = 501");
    CHECK(overview.demoApplicantCount == 500, "demo count equals 500");
    CHECK(overview.hkMaxFeasible > 0, "overview calculates HK upper bound cleanly");

    // 4. Run ALL 4 untouched AdminAllocator modes on the 501-applicant cohort
    ScoringWeights w1{0.65, 0.45, 0.35, 0.20};
    AdminSimulationRequest req;
    req.cycle = "2026–27 · Fall Term";
    req.weights = w1;
    req.allowUnranked = false;
    req.shortlistLimit = 10;

    req.mode = "best_fit";
    AdminSimulationResult resBestFit = admin.simulate(req);
    std::printf("  INFO  501-cohort best_fit: placed=%d/%d, 1st=%.1f%%, avg=%.3f, time=%.1f ms\n",
                resBestFit.placedCount, resBestFit.hkMax, resBestFit.firstChoicePct,
                resBestFit.avgScore, resBestFit.solveTimeMs);
    CHECK(resBestFit.valid && resBestFit.placedCount == resBestFit.hkMax && resBestFit.placedCount > 100,
          "best_fit on 501 cohort achieves HK cardinality and valid placements");

    req.mode = "most_placed";
    AdminSimulationResult resHk = admin.simulate(req);
    std::printf("  INFO  501-cohort most_placed (HK): placed=%d, time=%.1f ms\n",
                resHk.placedCount, resHk.solveTimeMs);
    CHECK(resHk.valid && resHk.placedCount == resBestFit.placedCount,
          "most_placed matches best_fit cardinality");

    req.mode = "highest_score";
    AdminSimulationResult resHigh = admin.simulate(req);
    std::printf("  INFO  501-cohort highest_score: placed=%d, avg=%.3f, time=%.1f ms\n",
                resHigh.placedCount, resHigh.avgScore, resHigh.solveTimeMs);
    CHECK(resHigh.valid && resHigh.placedCount > 0,
          "highest_score completes on 501 cohort");

    req.mode = "shortlists";
    AdminSimulationResult resSl = admin.simulate(req);
    std::printf("  INFO  501-cohort shortlists (Heap): lists=%d, time=%.1f ms\n",
                resSl.shortlists.size(), resSl.solveTimeMs);
    CHECK(resSl.valid && resSl.shortlists.size() > 0,
          "shortlists (Heap) completes on 501 cohort");

    req.mode = "best_fit";
    req.allowUnranked = true;
    AdminSimulationResult resUnranked = admin.simulate(req);
    std::printf("  INFO  501-cohort best_fit (allowUnranked=true): placed=%d/%d, unranked=%d, time=%.1f ms\n",
                resUnranked.placedCount, resUnranked.hkMax, resUnranked.unrankedCount, resUnranked.solveTimeMs);
    CHECK(resUnranked.valid && resUnranked.placedCount >= resBestFit.placedCount,
          "best_fit with allowUnranked=true places at least as many students as ranked-only");
    req.allowUnranked = false;

    // 5. Save 2 runs, compute diff, and publish
    req.mode = "best_fit";
    AdminSavedRunDetail run1;
    CHECK(admin.saveRun("Balanced Baseline", req, "n.hassan@aucegypt.edu", "2026-11-22T12:15:00", run1, err),
          "saves Run #1");

    req.weights = {0.20, 0.90, 0.25, 0.10};
    AdminSavedRunDetail run2;
    CHECK(admin.saveRun("GPA Priority Run", req, "n.hassan@aucegypt.edu", "2026-11-22T12:16:00", run2, err),
          "saves Run #2 with GPA emphasis");

    AdminSavedRunDetail loadedDetail;
    CHECK(admin.getSavedRunDetail(run1.summary.id, loadedDetail), "loads saved Run #1 detail");
    CHECK(loadedDetail.placements.size() == run1.placements.size(),
          "loaded run detail preserves placements count");
    CHECK(loadedDetail.universityFills.size() > 0,
          "loaded run detail populates universityFills metrics in C++");
    bool hasFilledWithCutoff = false;
    for (int i = 0; i < loadedDetail.universityFills.size(); ++i)
    {
        if (loadedDetail.universityFills[i].filledSeats > 0 &&
            loadedDetail.universityFills[i].hasCutoff)
        {
            hasFilledWithCutoff = true;
            break;
        }
    }
    CHECK(hasFilledWithCutoff, "loaded universityFills includes filled seat counts and cutoffs");

    AdminRunDiff diff;
    CHECK(admin.diffRuns(run2.summary.id, run1.summary.id, diff, err), "computes diff between Run #2 and Run #1");
    CHECK(diff.hasBaseline && (diff.shiftedCount > 0 || diff.gainedCount > 0 || diff.lostCount > 0),
          "weight shift changes placements between saved runs");

    AdminSavedRunSummary pub;
    CHECK(admin.publishRun(run2.summary.id, "2026-11-22T12:20:00", pub, err), "publishes Run #2");
    CHECK(pub.status == "Published", "published run has Published status");

    CHECK(admin.deleteRun(run1.summary.id, err), "deletes saved Run #1 cleanly");
    AdminSavedRunDetail deletedCheck;
    CHECK(!admin.getSavedRunDetail(run1.summary.id, deletedCheck),
          "deleted Run #1 is no longer loadable");
    Vector<AdminSavedRunSummary> remainingRuns = admin.listSavedRuns();
    CHECK(remainingRuns.size() == 1 && remainingRuns[0].id == run2.summary.id,
          "only Run #2 remains after deleting Run #1");
    CHECK(!admin.deleteRun(999999, err), "deleting missing run returns false");

    // 6. Clean clear of demo data must keep real.student@aucegypt.edu intact
    int removed = 0;
    CHECK(admin.clearDemoCohort(removed, err), "clears demo cohort cleanly");
    CHECK(removed == 500, "removed exactly 500 demo records");

    AdminApplicationFilter af;
    int remainingTotal = 0;
    Vector<AdminApplicationRow> remaining = admin.listApplications(af, remainingTotal);
    CHECK(remainingTotal == 1 && remaining[0].studentEmail == "real.student@aucegypt.edu",
          "non-demo real student survives demo clear intact");

    AdminOverviewStats afterClear = admin.getOverviewStats("2026–27 · Fall Term");
    CHECK(afterClear.demoApplicantCount == 0 && afterClear.totalApplicants == 1 &&
              afterClear.reviewQueueWaiting == 0,
          "overview updates accurately after clearing demo cohort");

    // 7. Reinstating the 500 demo cohort works repeatedly and preserves real student
    int reseeded = 0;
    CHECK(admin.seedDemoCohort("2026-11-22T12:25:00", reseeded, err),
          "reinstates 500 demo applicants after clearing");
    CHECK(reseeded == 500, "reinstated demo count is 500");

    AdminOverviewStats afterReinstate = admin.getOverviewStats("2026–27 · Fall Term");
    CHECK(afterReinstate.totalApplicants == 501 && afterReinstate.demoApplicantCount == 500,
          "overview restores 500 demo + 1 real applicant after reinstate");

    AdminApplicationRow realAfter;
    CHECK(admin.getApplicationDetail("real.student@aucegypt.edu", realAfter) &&
              realAfter.status == "Verified",
          "real student remains Verified after demo reinstate");

    sqlite3_close(db);
    std::printf("test_admin_service complete: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
