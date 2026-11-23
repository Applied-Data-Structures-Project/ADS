#include "services/admin_scoring.hpp"

#include <string>



// Preference factor by rank: 1st choice, 2nd choice, 3rd choice, unranked.
// Change these four numbers to change how much a student's choice order matters.
static const double PREFERENCE_FACTORS[4] = {1.0, 0.7, 0.4, 0.1};

// AUC's minimum GPA. Also the "floor" of the GPA factor (see gpaFactor).
static const double AUC_MIN_GPA = 2.8;

// The highest possible GPA. A student with this GPA gets GPA factor 1.
static const double MAX_GPA = 4.0;

// The statement rating is out of 10.
static const double MAX_STATEMENT_RATING = 10.0;

namespace
{


// Forces a number into the range 0..1 (below 0 becomes 0, above 1 becomes 1).
double clamp01(double value)
{
    if (value < 0.0)
        return 0.0;
    if (value > 1.0)
        return 1.0;
    return value;
}

// True if the list contains the given text (exact match).
bool containsText(const Vector<std::string> &list, const std::string &text)
{
    for (int i = 0; i < list.size(); ++i)
    {
        if (list[i] == text)
            return true;
    }
    return false;
}

// Finds which rank (0, 1 or 2) the student gave this university.
// Returns -1 if the university is not one of the student's 3 choices.
// An empty slot in prefs is skipped, so an empty slug never matches by accident.
int rankOf(const AdminStudent &student, const std::string &slug)
{
    for (int i = 0; i < 3; ++i)
    {
        if (!student.prefs[i].empty() && student.prefs[i] == slug)
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Eligibility checks. Each one returns true when the student PASSES the check.
// ---------------------------------------------------------------------------

// Check 1: the application must not be marked "Ineligible".
bool passesStatus(const AdminStudent &student)
{
    return student.status != "Ineligible";
}

// Check 2: the student's GPA must reach the AUC minimum.
bool passesAucGpa(const AdminStudent &student)
{
    return student.gpa >= AUC_MIN_GPA;
}

// Check 3: the student's GPA must reach the university's minimum.
// If the minimum is unknown we let the student through.
bool passesHostGpa(const AdminStudent &student, const AdminUniversity &university)
{
    if (!university.minGpaKnown)
        return true;
    return student.gpa >= university.minGpa;
}

// Check 4: the university must accept the student's major.
// An empty list (unknown) or a list containing "ALL" accepts everyone.
bool passesMajor(const AdminStudent &student, const AdminUniversity &university)
{
    if (university.acceptedMajors.size() == 0)
        return true;
    if (containsText(university.acceptedMajors, "ALL"))
        return true;
    return containsText(university.acceptedMajors, student.majorCode);
}

// Check 5: the university must offer the semester the student wants.
// An empty list (unknown) lets the student through.
bool passesSemester(const AdminStudent &student, const AdminUniversity &university)
{
    if (university.semesters.size() == 0)
        return true;
    return containsText(university.semesters, student.semester);
}

// Check 6: the university must be one of the student's 3 choices,
// unless the admin has allowed unranked matches.
bool passesRanking(int rank, bool allowUnranked)
{
    return rank >= 0 || allowUnranked;
}

// Runs the 6 checks in order and returns the FIRST one that fails.
// Returns Eligible if all of them pass.
PairReason checkEligibility(const AdminStudent &student,
                            const AdminUniversity &university,
                            int rank,
                            bool allowUnranked)
{
    if (!passesStatus(student))
        return PairReason::ApplicationIneligible;
    if (!passesAucGpa(student))
        return PairReason::BelowAucMinGpa;
    if (!passesHostGpa(student, university))
        return PairReason::BelowHostMinGpa;
    if (!passesMajor(student, university))
        return PairReason::MajorNotAccepted;
    if (!passesSemester(student, university))
        return PairReason::SemesterNotOffered;
    if (!passesRanking(rank, allowUnranked))
        return PairReason::NotRanked;
    return PairReason::Eligible;
}

// ---------------------------------------------------------------------------
// Factor conversions. Each one returns a number from 0 to 1.
// ---------------------------------------------------------------------------

// Preference factor: looks up the rank in PREFERENCE_FACTORS.
// rank -1 (not one of the 3 choices) uses the last entry, the "unranked" value.
double preferenceFactor(int rank)
{
    if (rank < 0)
        return PREFERENCE_FACTORS[3];
    return PREFERENCE_FACTORS[rank];
}

// GPA factor: how far the GPA is between 2.8 and 4.0, clamped to 0..1.
// We always use 2.8 as the floor (not the university's minimum), so the same
// student gets the same GPA factor at every university.
double gpaFactor(const AdminStudent &student)
{
    return clamp01((student.gpa - AUC_MIN_GPA) / (MAX_GPA - AUC_MIN_GPA));
}

// Statement factor: the rating divided by 10, clamped to 0..1.
// A student with no rating yet gets 0 (we never invent a rating).
double statementFactor(const AdminStudent &student)
{
    if (!student.hasStatementRating)
        return 0.0;
    return clamp01(student.statementRating / MAX_STATEMENT_RATING);
}

// Fairness factor: 1 for a student who has never studied abroad, 0 otherwise.
double fairnessFactor(const AdminStudent &student)
{
    return student.studiedAbroadBefore ? 0.0 : 1.0;
}

}
// ---------------------------------------------------------------------------
// Public functions (declared in admin_scoring.hpp)
// ---------------------------------------------------------------------------

// Turns a reason into a plain-English sentence for the admin to read.
const char *reasonText(PairReason reason)
{
    switch (reason)
    {
    case PairReason::Eligible:
        return "Eligible";
    case PairReason::ApplicationIneligible:
        return "The application is marked Ineligible";
    case PairReason::BelowAucMinGpa:
        return "GPA is below the AUC minimum of 2.8";
    case PairReason::BelowHostMinGpa:
        return "GPA is below the university's minimum";
    case PairReason::MajorNotAccepted:
        return "The university does not accept this major";
    case PairReason::SemesterNotOffered:
        return "The university does not offer the wanted semester";
    case PairReason::NotRanked:
        return "The university is not one of the student's 3 choices";
    }
    return "Unknown reason";
}


bool AdminFitScorer::weightsAreValid(const ScoringWeights &weights)
{
    if (weights.preference < 0.0 || weights.gpa < 0.0 ||
        weights.statement < 0.0 || weights.fairness < 0.0)
        return false;
    double sum = weights.preference + weights.gpa + weights.statement + weights.fairness;
    return sum > 0.0;
}

// Weighted average of the four factors. Dividing by the sum of the weights
// keeps the result between 0 and 1 however the sliders are set.
double AdminFitScorer::blendScore(const FactorScores &factors, const ScoringWeights &weights)
{
    if (!weightsAreValid(weights))
        return 0.0;
    double sum = weights.preference + weights.gpa + weights.statement + weights.fairness;
    double top = weights.preference * factors.preference +
                 weights.gpa * factors.gpa +
                 weights.statement * factors.statement +
                 weights.fairness * factors.fairness;
    return top / sum;
}

// Stage 1 (constructor): builds one PairResult for every student/university pair.

AdminFitScorer::AdminFitScorer(const Vector<AdminStudent> &students,
                               const Vector<AdminUniversity> &universities,
                               bool allowUnranked,
                               const ScoringWeights &weights)
    : applied(false)
{
    for (int s = 0; s < students.size(); ++s)
    {
        for (int u = 0; u < universities.size(); ++u)
        {
            const AdminStudent &student = students[s];
            const AdminUniversity &university = universities[u];
            int rank = rankOf(student, university.slug);

            PairResult result;
            result.studentIndex = s;
            result.universityIndex = u;
            result.reason = checkEligibility(student, university, rank, allowUnranked);

            // Factors and flags only make sense for pairs that are allowed.
            if (result.eligible())
            {
                result.factors.preference = preferenceFactor(rank);
                result.factors.gpa = gpaFactor(student);
                result.factors.statement = statementFactor(student);
                result.factors.fairness = fairnessFactor(student);
                result.minGpaUnknown = !university.minGpaKnown;
                result.statementMissing = !student.hasStatementRating;
            }
            results.push_back(result);
        }
    }
    setWeights(weights);
}

// Stage 2: recomputes the score of every eligible pair from its stored factors.
// Ineligible pairs keep score 0. Invalid weights change nothing and return false.
bool AdminFitScorer::setWeights(const ScoringWeights &weights)
{
    if (!weightsAreValid(weights))
        return false;
    currentWeights = weights;
    for (int i = 0; i < results.size(); ++i)
    {
        if (results[i].eligible())
            results[i].score = blendScore(results[i].factors, currentWeights);
    }
    applied = true;
    return true;
}

// True once valid weights have been applied.
bool AdminFitScorer::weightsApplied() const
{
    return applied;
}

// How many results there are (number of students times number of universities).
int AdminFitScorer::pairCount() const
{
    return results.size();
}

// One result by position.
const PairResult &AdminFitScorer::pair(int index) const
{
    return results[index];
}

// All results, read-only.
const Vector<PairResult> &AdminFitScorer::pairs() const
{
    return results;
}

// The weights the scores are currently based on.
const ScoringWeights &AdminFitScorer::weights() const
{
    return currentWeights;
}
