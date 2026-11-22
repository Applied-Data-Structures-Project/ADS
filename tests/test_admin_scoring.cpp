// Self-contained test for AdminFitScorer. No test framework.
// main() returns 0 when every check passes, and 1 when any check fails.
// Each failed check prints its name so you can see which one broke.

#include <cmath>
#include <iostream>
#include <string>

#include "vector.hpp"
#include "services/admin_scoring.hpp"

// ---------------------------------------------------------------------------
// Tiny test helpers
// ---------------------------------------------------------------------------

static int failures = 0;

// Records a failure (and prints its name) when the condition is false.
static void check(bool condition, const std::string &name)
{
    if (!condition)
    {
        std::cout << "FAIL: " << name << "\n";
        ++failures;
    }
}

// True when two numbers are equal within the given tolerance.
static bool near(double a, double b, double tolerance)
{
    return std::fabs(a - b) <= tolerance;
}

// Builds a ScoringWeights from four numbers.
static ScoringWeights makeWeights(double pref, double gpa, double statement, double fairness)
{
    ScoringWeights w;
    w.preference = pref;
    w.gpa = gpa;
    w.statement = statement;
    w.fairness = fairness;
    return w;
}

// The default weights from the worked example.
static ScoringWeights defaultWeights()
{
    return makeWeights(0.65, 0.45, 0.35, 0.20);
}

// A student who passes every check for "uva" as their first choice.
// Tests change one thing at a time from this starting point.
static AdminStudent baseStudent()
{
    AdminStudent s;
    s.id = 1;
    s.name = "Base";
    s.gpa = 3.5;
    s.majorCode = "CSCE";
    s.semester = "Fall";
    s.prefs[0] = "uva";
    s.statementRating = 8.0;
    s.hasStatementRating = true;
    s.studiedAbroadBefore = false;
    return s;
}

// A university with no restrictions at all.
static AdminUniversity baseUniversity()
{
    AdminUniversity u;
    u.slug = "uva";
    u.name = "University of Virginia";
    return u;
}

// Runs the scorer for ONE student and ONE university and returns the one result.
static PairResult scoreOne(const AdminStudent &student, const AdminUniversity &university,
                           bool allowUnranked)
{
    Vector<AdminStudent> students;
    students.push_back(student);
    Vector<AdminUniversity> universities;
    universities.push_back(university);
    AdminFitScorer scorer(students, universities, allowUnranked, defaultWeights());
    return scorer.pair(0);
}

// ---------------------------------------------------------------------------
// a) The worked example, and re-blending on the SAME object
// ---------------------------------------------------------------------------

// Makes a student for the worked example. uvaRank is 0, 1 or 2 (where uva sits).
static AdminStudent exampleStudent(const std::string &name, double gpa, double statement, int uvaRank)
{
    AdminStudent s = baseStudent();
    s.name = name;
    s.gpa = gpa;
    s.statementRating = statement;
    s.prefs[0] = "";
    s.prefs[uvaRank] = "uva";
    return s;
}

static void testWorkedExample()
{
    Vector<AdminStudent> students;
    students.push_back(exampleStudent("Youssef", 3.78, 8.9, 0));
    students.push_back(exampleStudent("Laila", 3.85, 9.0, 1));
    students.push_back(exampleStudent("Salma", 3.92, 9.2, 2));

    AdminUniversity uva = baseUniversity();
    uva.acceptedMajors.push_back("ALL");
    Vector<AdminUniversity> universities;
    universities.push_back(uva);

    AdminFitScorer scorer(students, universities, false, defaultWeights());
    check(scorer.weightsApplied(), "worked example: default weights applied");
    check(scorer.pairCount() == 3, "worked example: 3 pairs");
    check(near(scorer.pair(0).score, 0.93, 0.005), "worked example: Youssef about 0.93");
    check(near(scorer.pair(1).score, 0.83, 0.005), "worked example: Laila about 0.83");
    check(near(scorer.pair(2).score, 0.73, 0.005), "worked example: Salma about 0.73");

    // Same object, new weights: only the blend is redone, the order reverses.
    bool changed = scorer.setWeights(makeWeights(0.10, 2.00, 0.35, 0.20));
    check(changed, "worked example: setWeights returns true");
    check(near(scorer.pair(0).score, 0.85, 0.005), "re-blend: Youssef about 0.85");
    check(near(scorer.pair(1).score, 0.88, 0.005), "re-blend: Laila about 0.88");
    check(near(scorer.pair(2).score, 0.92, 0.005), "re-blend: Salma about 0.92");
    check(scorer.pair(2).score > scorer.pair(0).score, "re-blend: order reversed");
    check(near(scorer.weights().gpa, 2.00, 0.0001), "re-blend: weights() shows new weights");
}

// ---------------------------------------------------------------------------
// b) One test per ineligible reason
// ---------------------------------------------------------------------------

static void testReasons()
{
    AdminStudent s = baseStudent();
    AdminUniversity u = baseUniversity();
    check(scoreOne(s, u, false).reason == PairReason::Eligible, "base pair is eligible");

    AdminStudent ineligible = baseStudent();
    ineligible.status = "Ineligible";
    check(scoreOne(ineligible, u, false).reason == PairReason::ApplicationIneligible,
          "reason: ApplicationIneligible");

    AdminStudent lowGpa = baseStudent();
    lowGpa.gpa = 2.5;
    check(scoreOne(lowGpa, u, false).reason == PairReason::BelowAucMinGpa,
          "reason: BelowAucMinGpa");

    AdminUniversity strict = baseUniversity();
    strict.minGpaKnown = true;
    strict.minGpa = 3.7;
    check(scoreOne(s, strict, false).reason == PairReason::BelowHostMinGpa,
          "reason: BelowHostMinGpa");

    AdminUniversity engineersOnly = baseUniversity();
    engineersOnly.acceptedMajors.push_back("ENGR");
    check(scoreOne(s, engineersOnly, false).reason == PairReason::MajorNotAccepted,
          "reason: MajorNotAccepted");

    AdminUniversity springOnly = baseUniversity();
    springOnly.semesters.push_back("Spring");
    check(scoreOne(s, springOnly, false).reason == PairReason::SemesterNotOffered,
          "reason: SemesterNotOffered");

    AdminUniversity other = baseUniversity();
    other.slug = "other";
    check(scoreOne(s, other, false).reason == PairReason::NotRanked,
          "reason: NotRanked");

    // An ineligible pair keeps score 0.
    check(scoreOne(lowGpa, u, false).score == 0.0, "ineligible pair has score 0");
}

// ---------------------------------------------------------------------------
// c) Unknown minimum GPA, d) missing statement, e) unranked
// ---------------------------------------------------------------------------

static void testUnknownMinGpa()
{
    AdminUniversity u = baseUniversity();  // minGpaKnown is false
    PairResult r = scoreOne(baseStudent(), u, false);
    check(r.eligible(), "unknown min GPA: student is let through");
    check(r.minGpaUnknown, "unknown min GPA: flag is set");

    u.minGpaKnown = true;
    u.minGpa = 3.0;
    check(!scoreOne(baseStudent(), u, false).minGpaUnknown, "known min GPA: flag is not set");
}

static void testMissingStatement()
{
    AdminStudent s = baseStudent();
    s.hasStatementRating = false;
    s.statementRating = 9.9;  // must be ignored because hasStatementRating is false
    PairResult r = scoreOne(s, baseUniversity(), false);
    check(r.eligible(), "missing statement: still eligible");
    check(r.factors.statement == 0.0, "missing statement: factor is 0");
    check(r.statementMissing, "missing statement: flag is set");
    check(!scoreOne(baseStudent(), baseUniversity(), false).statementMissing,
          "rated statement: flag is not set");
}

static void testUnranked()
{
    AdminUniversity other = baseUniversity();
    other.slug = "other";

    PairResult allowed = scoreOne(baseStudent(), other, true);
    check(allowed.eligible(), "unranked allowed: eligible");
    check(near(allowed.factors.preference, 0.1, 0.0001), "unranked allowed: preference factor 0.1");

    PairResult blocked = scoreOne(baseStudent(), other, false);
    check(blocked.reason == PairReason::NotRanked, "unranked not allowed: NotRanked");
}

// ---------------------------------------------------------------------------
// f) Invalid weights
// ---------------------------------------------------------------------------

// Builds a scorer for one eligible pair with the default weights.
static AdminFitScorer oneEligibleScorer()
{
    Vector<AdminStudent> students;
    students.push_back(baseStudent());
    Vector<AdminUniversity> universities;
    universities.push_back(baseUniversity());
    return AdminFitScorer(students, universities, false, defaultWeights());
}

static void testInvalidWeights()
{
    check(!AdminFitScorer::weightsAreValid(makeWeights(0, 0, 0, 0)), "all-zero weights are invalid");
    check(!AdminFitScorer::weightsAreValid(makeWeights(1, -0.1, 1, 1)), "negative weight is invalid");
    check(AdminFitScorer::weightsAreValid(defaultWeights()), "default weights are valid");
    check(AdminFitScorer::blendScore(FactorScores(), makeWeights(0, 0, 0, 0)) == 0.0,
          "blendScore with invalid weights returns 0");

    AdminFitScorer scorer = oneEligibleScorer();
    double oldScore = scorer.pair(0).score;
    check(oldScore > 0.0, "invalid weights test: starts with a real score");

    check(!scorer.setWeights(makeWeights(0, 0, 0, 0)), "all-zero setWeights returns false");
    check(scorer.pair(0).score == oldScore, "all-zero setWeights leaves score unchanged");
    check(near(scorer.weights().preference, 0.65, 0.0001), "all-zero setWeights leaves weights unchanged");

    check(!scorer.setWeights(makeWeights(1, -1, 1, 1)), "negative setWeights returns false");
    check(scorer.pair(0).score == oldScore, "negative setWeights leaves score unchanged");
    check(scorer.weightsApplied(), "weightsApplied stays true after a refused change");

    // Constructing with invalid weights: object exists, nothing applied, scores 0.
    Vector<AdminStudent> students;
    students.push_back(baseStudent());
    Vector<AdminUniversity> universities;
    universities.push_back(baseUniversity());
    AdminFitScorer bad(students, universities, false, makeWeights(0, 0, 0, 0));
    check(!bad.weightsApplied(), "constructed with invalid weights: weightsApplied is false");
    check(bad.pair(0).score == 0.0, "constructed with invalid weights: score is 0");
    check(bad.pair(0).eligible(), "constructed with invalid weights: pair is still evaluated");

    // Valid weights can be applied later on the same object.
    check(bad.setWeights(defaultWeights()), "valid setWeights after invalid construction returns true");
    check(bad.weightsApplied() && bad.pair(0).score > 0.0, "valid weights applied later give a score");
}

int main()
{
    testWorkedExample();
    testReasons();
    testUnknownMinGpa();
    testMissingStatement();
    testUnranked();
    testInvalidWeights();

    if (failures == 0)
        std::cout << "All admin scoring checks passed\n";
    else
        std::cout << failures << " check(s) failed\n";
    return failures == 0 ? 0 : 1;
}
