#pragma once
// Admin "fit score" service: declarations only (the code is in admin_scoring.cpp).
//
// DESIGN
// For every (student, university) pair we answer two SEPARATE questions:
//   (a) Is the student ALLOWED to go there?  Yes/no. If no, we record the reason.
//   (b) If allowed, how good is the fit?     One number from 0 to 1.
// The matching algorithms (a later step) only see the allowed pairs and their
// scores. The matcher never sees GPA directly.
//
// All of this lives in one class, AdminFitScorer. Its work is split into two
// stages so moving a slider is cheap:
//   1. The CONSTRUCTOR runs the eligibility checks and converts the raw data
//      into four factor numbers (0..1) for every pair. Done once.
//   2. setWeights() blends the stored factors with the slider weights into one
//      score. Done again every time a slider moves. It does NOT redo the
//      eligibility checks or the factor conversions.
// That is why each PairResult stores the four factors, not just the final score.
//
// The scorer COPIES what it needs. It keeps no references or pointers to the
// students/universities Vectors that were passed in, so nothing can dangle.
//
// Unknown data never blocks a student: an unknown minimum GPA, an empty
// accepted-majors list, or an empty semester list counts as "check passed".
//
// This file must not include Crow (HTTP) or SQLite, so it can be tested alone.

#include "vector.hpp"
#include "models/admin_models.hpp"

// Why a pair is allowed or not. The first failing check is the one recorded.
enum class PairReason
{
    Eligible,
    ApplicationIneligible,  // 1. application status is "Ineligible"
    BelowAucMinGpa,         // 2. GPA below the AUC minimum (2.8)
    BelowHostMinGpa,        // 3. GPA below the university's known minimum
    MajorNotAccepted,       // 4. university does not accept the student's major
    SemesterNotOffered,     // 5. university does not offer the wanted semester
    NotRanked               // 6. not one of the 3 choices and unranked not allowed
};

// The four factors, each already converted to a number from 0 to 1.
struct FactorScores
{
    double preference = 0.0;
    double gpa = 0.0;
    double statement = 0.0;
    double fairness = 0.0;
};

// The result for one (student, university) pair.
struct PairResult
{
    int studentIndex = 0;     // position in the students Vector
    int universityIndex = 0;  // position in the universities Vector
    PairReason reason = PairReason::Eligible;
    FactorScores factors;     // only filled in for eligible pairs
    double score = 0.0;       // the blended score; 0 until valid weights are applied
    bool minGpaUnknown = false;   // eligible, but the university's min GPA is unknown
    bool statementMissing = false; // the student has no statement rating yet

    // Small helper: true when the student is allowed to go to this university.
    bool eligible() const { return reason == PairReason::Eligible; }
};

// Plain-English sentence for a reason, e.g. "GPA is below the AUC minimum of 2.8".
const char *reasonText(PairReason reason);

// Holds one PairResult for every (student, university) pair and keeps the
// scores up to date when the admin changes the weights.
class AdminFitScorer
{
public:
    // Stage 1: builds one result for EVERY pair, eligible or not.
    // allowUnranked = true lets a student be matched to a university outside
    // their 3 choices (with a low preference factor).
    // If the weights are invalid the object is still created, every score
    // stays 0, and weightsApplied() returns false.
    AdminFitScorer(const Vector<AdminStudent> &students,
                   const Vector<AdminUniversity> &universities,
                   bool allowUnranked,
                   const ScoringWeights &weights);

    // Stage 2: if the weights are valid, stores them, recomputes the score of
    // every eligible pair from its stored factors, and returns true.
    // If they are invalid, changes nothing and returns false.
    bool setWeights(const ScoringWeights &weights);

    // True once valid weights have been applied.
    bool weightsApplied() const;

    int pairCount() const;                    // number of results (students x universities)
    const PairResult &pair(int index) const;  // one result by position
    const Vector<PairResult> &pairs() const;  // all results
    const ScoringWeights &weights() const;    // the weights currently in use

    // True when all four weights are >= 0 and their sum is greater than 0.
    static bool weightsAreValid(const ScoringWeights &weights);

    // Blends four factors into one score between 0 and 1 using the weights.
    // Returns 0 if the weights are invalid (so it never divides by zero).
    static double blendScore(const FactorScores &factors, const ScoringWeights &weights);

private:
    Vector<PairResult> results;     // one entry per pair
    ScoringWeights currentWeights;  // the weights the scores were last computed with
    bool applied;                   // true once valid weights have been applied
};
