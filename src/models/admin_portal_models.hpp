#pragma once
// Plain C++ data structs for the Admin Portal integration layer (src/services/admin_service.*
// and src/repo/admin_repo.*). No SQLite, Crow, or STL container headers allowed.

#include <string>
#include "vector.hpp"
#include "models/admin_models.hpp"
#include "models/application.hpp"
#include "models/student_profile.hpp"

// Admin review annotations layered on top of a student's application.
struct AdminReviewRecord
{
    std::string studentEmail;
    bool hasStatementRating = false;
    double statementRating = 0.0;
    std::string notes;
    bool isDemo = false;
    std::string updatedAt;
};

// Full partner university dossier (backed by SQLite `universities` + `admin_seat_overrides`).
struct AdminPartnerDetail
{
    std::string slug;
    std::string officialName;
    std::string shortName;
    std::string country;
    std::string stateOrProvince;
    std::string city;
    std::string region;
    std::string agreementStatus;
    std::string agreementScope;
    Vector<std::string> studentLevels;
    Vector<std::string> eligibleAucSchools;
    Vector<std::string> eligibleMajors;
    Vector<std::string> excludedMajors;
    Vector<std::string> availableTerms;
    bool minGpaKnown = false;
    double minGpa = 0.0;
    int seatQuotaSeeded = -1; // -1 if NULL in universities table
    int seats = 2;            // effective seat quota (override -> seeded -> default)
    bool seatsAssumed = true; // true if using default seat count (no explicit override/seed)
    bool livingCostUsdKnown = false;
    double livingCostUsd = 0.0;
    std::string estMonthlyLocal; // e.g. "1000 EUR/mo" or "39934-74272 JPY/mo"
    std::string tuitionModel;
    std::string housingAvailability;
    std::string factSheetStatus;
    int factSheetFilesCount = 0;

    // Live applicant demand calculated from applications
    int firstChoiceDemand = 0;
    int totalDemand = 0;

    // Detailed dossier fields
    std::string minStandingNote;
    Vector<std::string> instructionLanguages;
    std::string englishCourseAvailability;
    std::string englishProficiency;
    std::string hostLanguageRequirement;
    std::string fallTermDates;
    std::string springTermDates;
    std::string nominationDeadlines;
    int finalSemesterAllowed = -1; // -1 = unknown, 0 = no, 1 = yes
    std::string creditSystemAndLoad;
    std::string courseRestrictionsNote;
    Vector<std::string> requiredDocs;
    std::string mandatoryHostFees;
    std::string housingNotes;
    std::string hostGrants;
    std::string citizenshipRestrictions;
    std::string visaNotes;
    std::string academicStyle;
    std::string internationalSupport;
    std::string officialWebsiteUrl;
    std::string exchangePageUrl;
    std::string courseCatalogUrl;
    std::string englishCourseListUrl;
};

struct AdminPartnerFilter
{
    std::string search;
    std::string region = "all";
    std::string agreementStatus = "all";
    std::string term = "all";
    std::string level = "all";
    std::string quotaState = "all"; // "all" | "custom" | "default" | "demanded"
};

struct AdminPartnerStats
{
    int totalPartners = 0;
    int activePartners = 0;
    int checkSlotsPartners = 0;
    int loiPartners = 0;
    int totalConfiguredSeats = 0;
    int customQuotaCount = 0;
    int highDemandCount = 0;
    int factSheetMatchedCount = 0;
    int knownMinGpaCount = 0;
};

// Joined view of a student application, profile eligibility intake, docs, letters, and admin annotations.
struct AdminApplicationRow
{
    std::string studentEmail;
    std::string firstName;
    std::string surname;
    std::string fullName;
    std::string aucId;
    std::string level;    // "UG" or "GR"
    std::string standing; // "Sophomore", "Junior", "Senior", "Graduate"
    std::string major;
    double gpa = 0.0;
    bool hasCredits = false;
    int credits = 0;
    bool majorDeclared = true;
    bool rhet110 = true;
    bool rhet120 = true;
    std::string rhet1020;
    std::string expectedGraduation;
    bool finalSemester = false;
    bool academicProbation = false;
    bool disciplinaryProbation = false;
    bool englishComplete = true;
    bool firstSemesterNoGpa = false;
    bool eligibilitySaved = false;

    std::string nationality;
    std::string secondNationality;
    std::string gender;
    std::string phone;
    std::string dateOfBirth;
    std::string programType;
    std::string scholarship;
    bool accommodation = false;
    int budget = 0;
    bool prior = false;
    std::string travelExperience;
    std::string status = "Draft"; // Draft, Submitted, In Review, Verified, Ineligible
    int goodStanding = -1;
    bool declaration = false;
    std::string submittedAt;
    std::string updatedAt;

    std::string prefSlugs[3];
    std::string prefNames[3];

    Vector<std::string> uploadedDocs; // doc_types uploaded
    int letterReceivedCount = 0;
    int letterPendingCount = 0;
    int letterDeclinedCount = 0;

    // Guardian & Questionnaire (for dossier inspection)
    std::string guardianName;
    std::string guardianRelation;
    std::string guardianPhone;
    std::string guardianEmail;
    std::string guardianLanguage;
    std::string qPreparedness;
    std::string qGoals;
    std::string qAdaptability;
    std::string qTeamwork;
    std::string qReflection;
    std::string qFinance;

    // Admin review metadata
    bool hasStatementRating = false;
    double statementRating = 0.0;
    std::string adminNotes;
    bool isDemo = false;
    bool flagged = false;
    std::string eligibilityReason; // "" if meets base AUC intake rules, otherwise reason
};

struct AdminApplicationFilter
{
    std::string cohort = "all"; // "all" | "eligible" | "flagged"
    std::string search;
    std::string standing = "all";
    std::string level = "all";
    double gpaMin = 0.0;
    double gpaMax = 4.0;
    std::string uniSlug = "all";
    std::string prior = "all"; // "all" | "first" | "prior"
    std::string major;
    std::string status = "all";
};

struct AdminReviewUpdate
{
    std::string status; // empty = keep existing
    bool hasGoodStanding = false;
    int goodStanding = -1; // -1, 0, or 1
    bool updateStatementRating = false;
    bool hasStatementRating = false;
    double statementRating = 0.0;
    bool updateNotes = false;
    std::string notes;
};

struct AdminOverviewStats
{
    int totalApplicants = 0;
    int eligibleCount = 0;
    int firstTimeAbroadCount = 0;
    int flaggedCount = 0;
    int lowGpaFlaggedCount = 0;
    int reviewQueueWaiting = 0;
    int demoApplicantCount = 0;
    int totalPartners = 0;
    int totalConfiguredSeats = 0;
    int customQuotaCount = 0;
    int hkMaxFeasible = 0;
};

struct AdminPlacementRow
{
    std::string studentEmail;
    std::string studentName;
    std::string studentAucId;
    std::string studentMajor;
    double studentGpa = 0.0;
    std::string universitySlug;
    std::string universityName;
    std::string seatLabel;
    int prefRank = 0; // 1, 2, 3, or 0 for unranked fallback
    std::string rankLabel;
    double score = 0.0;
};

struct AdminUnplacedRow
{
    std::string studentEmail;
    std::string studentName;
    std::string studentAucId;
    std::string studentMajor;
    double studentGpa = 0.0;
    std::string prefsSummary;
    std::string bottleneck; // "Capacity Contention" | "Eligibility / Policy" | "No Preferences / Seats"
    std::string reason;
};

struct AdminUniversityFillRow
{
    std::string slug;
    std::string name;
    std::string country;
    std::string region;
    int totalSeats = 0;
    bool seatsAssumed = true;
    int filledSeats = 0;
    int leftSeats = 0;
    int demandCount = 0;
    double demandRatio = 0.0;
    bool hasCutoff = false;
    double cutoffScore = 0.0;
};

struct AdminShortlistCandidateRow
{
    int rank = 0;
    std::string studentEmail;
    std::string studentName;
    std::string studentAucId;
    std::string studentMajor;
    double studentGpa = 0.0;
    double score = 0.0;
    bool minGpaUnknown = false;
    bool statementMissing = false;
};

struct AdminShortlistRow
{
    std::string slug;
    std::string name;
    std::string country;
    int totalSeats = 0;
    bool seatsAssumed = true;
    int eligibleCount = 0;
    Vector<AdminShortlistCandidateRow> candidates;
};

struct AdminRunDiffStudent
{
    std::string studentEmail;
    std::string studentName;
    std::string changeType; // "gained" | "lost" | "shifted"
    std::string fromUniSlug;
    std::string fromUniName;
    std::string toUniSlug;
    std::string toUniName;
    double scoreDelta = 0.0;
};

struct AdminRunDiff
{
    bool hasBaseline = false;
    int baselineRunId = 0;
    std::string baselineRunCode;
    std::string baselineTitle;
    int placedDelta = 0;
    int firstChoiceDelta = 0;
    int gainedCount = 0;
    int lostCount = 0;
    int shiftedCount = 0;
    Vector<AdminRunDiffStudent> changes;
};

struct AdminSimulationRequest
{
    std::string cycle = "2026–27 · Fall Term";
    std::string mode = "best_fit"; // "best_fit" | "highest_score" | "most_placed" | "shortlists"
    ScoringWeights weights;
    bool allowUnranked = false;
    int defaultSeats = 2;
    int shortlistLimit = 10;
    int compareRunId = 0; // 0 = auto compare with latest saved run if one exists
};

struct AdminSimulationResult
{
    bool valid = false;
    std::string error;
    std::string mode;
    std::string cycle;
    bool allowUnranked = false;
    ScoringWeights weights;
    int placedCount = 0;
    int eligibleCount = 0;
    int unplacedCount = 0;
    int hkMax = 0;
    int firstChoiceCount = 0;
    int secondChoiceCount = 0;
    int thirdChoiceCount = 0;
    int unrankedCount = 0;
    double firstChoicePct = 0.0;
    double avgScore = 0.0;
    double solveTimeMs = 0.0;
    Vector<AdminPlacementRow> placements;
    Vector<AdminUnplacedRow> unplaced;
    Vector<AdminUniversityFillRow> universityFills;
    Vector<AdminShortlistRow> shortlists;
    AdminRunDiff diff;
};

struct AdminSavedRunSummary
{
    int id = 0;
    std::string runCode;
    std::string cycle;
    std::string title;
    std::string mode;
    bool allowUnranked = false;
    ScoringWeights weights;
    int defaultSeats = 2;
    std::string status = "Draft"; // Draft, Published, Archived
    int placedCount = 0;
    int eligibleCount = 0;
    int hkMax = 0;
    int firstChoiceCount = 0;
    int secondChoiceCount = 0;
    int thirdChoiceCount = 0;
    int unrankedCount = 0;
    double firstChoicePct = 0.0;
    double avgScore = 0.0;
    std::string createdBy;
    std::string createdAt;
};

struct AdminSavedRunDetail
{
    AdminSavedRunSummary summary;
    Vector<AdminPlacementRow> placements;
    Vector<AdminUnplacedRow> unplaced;
    Vector<AdminUniversityFillRow> universityFills;
};
