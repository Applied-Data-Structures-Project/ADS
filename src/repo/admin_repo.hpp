#pragma once
// Admin SQLite repository: pure persistence layer for /api/admin/*.
// Reads live universities, students, applications, documents, and letters;
// reads/writes admin_seat_overrides, admin_application_reviews, admin_match_runs,
// admin_match_assignments, and admin_match_unplaced.
// No business logic, no Crow headers, no STL containers.

#include <string>
#include "sqlite3.h"
#include "vector.hpp"
#include "models/admin_portal_models.hpp"
#include "models/admin_seat_models.hpp"
#include "models/application.hpp"
#include "models/student_profile.hpp"

class AdminRepo
{
public:
    explicit AdminRepo(sqlite3 *db);

    // Partner universities (seeded `universities` table + `admin_seat_overrides`)
    Vector<AdminPartnerDetail> loadPartners();
    Vector<AdminSeatCount> loadSeatOverrides();
    bool saveSeatOverride(const std::string &slug, int seats, const std::string &updatedAt);

    // Student & application live snapshots
    Vector<StudentRecord> loadStudents();
    Vector<Application> loadApplications();
    Vector<ApplicationDocument> loadDocuments();
    Vector<LetterRequest> loadLetters();
    bool updateApplicationReviewFields(const std::string &email,
                                       const std::string &status,
                                       int goodStanding,
                                       const std::string &updatedAt);

    // Admin review metadata (`admin_application_reviews`)
    Vector<AdminReviewRecord> loadReviews();
    bool saveReview(const AdminReviewRecord &review);

    // Saved matching runs (`admin_match_runs`, `admin_match_assignments`, `admin_match_unplaced`)
    Vector<AdminSavedRunSummary> loadSavedRuns();
    bool loadSavedRunDetail(int runId, AdminSavedRunDetail &out);
    bool insertSavedRun(AdminSavedRunDetail &run);
    bool archivePublishedRuns();
    bool updateRunStatus(int runId, const std::string &status);
    bool deleteSavedRun(int runId);

    // Transactional demo cohort seeding & isolation cleanup (`is_demo = 1`)
    bool seedDemoBatch(const Vector<StudentRecord> &students,
                       const Vector<Application> &applications,
                       const Vector<ApplicationDocument> &documents,
                       const Vector<LetterRequest> &letters,
                       const Vector<AdminReviewRecord> &reviews);
    int clearDemoBatch();

private:
    sqlite3 *db;
};
