#pragma once
// Admin portal service: orchestrates partner university management, applicant audit &
// review workflow, bipartite matching simulations (via untouched AdminAllocator /
// AdminFitScorer / AdminSeatTable), saved run persistence & diffs, and demo cohort stress data.
//
// Strictly free of Crow HTTP types, SQLite C API calls, and STL containers
// (uses only custom Vector, HashMap, Heap, Queue + std::string).

#include <string>
#include "hashmap.hpp"
#include "vector.hpp"
#include "models/admin_portal_models.hpp"
#include "repo/admin_repo.hpp"
#include "services/application_service.hpp"
#include "services/discovery_service.hpp"

class AdminService
{
public:
    AdminService(AdminRepo &adminRepo,
                 ApplicationService &appService,
                 DiscoveryService &discoveryService);

    // Loads partner catalog, seat overrides, admin review metadata, and saved runs at startup.
    void loadAll();

    // Overview KPIs (fast Hopcroft-Karp hkMaxFeasible computed live across active applicants).
    AdminOverviewStats getOverviewStats(const std::string &cycle) const;

    // Partner universities (real 155 seeded partners + persistent seat overrides + demand).
    Vector<AdminPartnerDetail> listPartners(const AdminPartnerFilter &filter,
                                            AdminPartnerStats &statsOut) const;
    bool getPartnerDetail(const std::string &slug, AdminPartnerDetail &out) const;
    bool setPartnerSeats(const std::string &slug,
                         int seats,
                         const std::string &now,
                         AdminPartnerDetail &out,
                         std::string &error);

    // Student applications & eligibility audit.
    Vector<AdminApplicationRow> listApplications(const AdminApplicationFilter &filter,
                                                 int &totalCountOut) const;
    bool getApplicationDetail(const std::string &email, AdminApplicationRow &out) const;
    bool updateApplicationReview(const std::string &email,
                                 const AdminReviewUpdate &update,
                                 const std::string &now,
                                 AdminApplicationRow &out,
                                 std::string &error);

    // Matching simulation (runs all 4 untouched AdminAllocator options).
    AdminSimulationResult simulate(const AdminSimulationRequest &request) const;

    // Saved matching runs, comparisons (diffs), and publishing.
    Vector<AdminSavedRunSummary> listSavedRuns() const;
    bool getSavedRunDetail(int runId, AdminSavedRunDetail &out) const;
    bool saveRun(const std::string &title,
                 const AdminSimulationRequest &request,
                 const std::string &createdBy,
                 const std::string &now,
                 AdminSavedRunDetail &out,
                 std::string &error);
    bool publishRun(int runId,
                    const std::string &now,
                    AdminSavedRunSummary &out,
                    std::string &error);
    bool deleteRun(int runId, std::string &error);
    bool diffRuns(int runIdA, int runIdB, AdminRunDiff &out, std::string &error) const;

    // Demo-mode stress-test cohort (500 realistic varied AUC applicants, is_demo = 1).
    bool seedDemoCohort(const std::string &now, int &seededCountOut, std::string &error);
    bool clearDemoCohort(int &removedCountOut, std::string &error);

private:
    AdminRepo &adminRepo;
    ApplicationService &appService;
    DiscoveryService &discoveryService;

    Vector<AdminPartnerDetail> partners;
    HashMap<std::string, int> partnerIndexBySlug;
    HashMap<std::string, int> seatOverrides;
    HashMap<std::string, AdminReviewRecord> reviews;
    Vector<AdminSavedRunSummary> savedRuns;

    Vector<AdminApplicationRow> buildAllApplicationRows() const;
    void computeDemandMaps(const Vector<AdminApplicationRow> &rows,
                           HashMap<std::string, int> &firstChoiceOut,
                           HashMap<std::string, int> &totalDemandOut) const;
    Vector<AdminUniversity> buildAdminUniversities() const;
    Vector<AdminSeatCount> buildSeatCounts() const;
    Vector<AdminStudent> buildAdminStudents(const Vector<AdminApplicationRow> &rows,
                                            const std::string &term,
                                            Vector<int> &sourceRowIndicesOut) const;
    AdminRunDiff computeDiff(const Vector<AdminPlacementRow> &currentPlacements,
                             int currentFirstChoices,
                             int baselineRunId) const;
    Vector<AdminUniversityFillRow> buildUniversityFillsFromPlacements(
        const Vector<AdminPlacementRow> &placements,
        int defaultSeats) const;
    std::string nextRunCode() const;
};
