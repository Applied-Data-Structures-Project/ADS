// Self-contained test for AdminAllocator. No test framework.
// main() returns 0 when every check passes, and 1 when any check fails.
// Each failed check prints its name so you can see which one broke.
//
// Only our own Vector and std::string are used for data. Random numbers come from
// a tiny hand-written generator with a fixed seed, so every run is the same.

#include <cmath>
#include <iostream>
#include <string>

#include "vector.hpp"
#include "services/admin_allocator.hpp"

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

// A tiny linear congruential generator. Same seed, same numbers, every time.
class Lcg
{
public:
    explicit Lcg(unsigned long long seed) : state(seed) {}

    // A number from 0 to 2^31 - 1.
    unsigned int next()
    {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<unsigned int>(state >> 33);
    }

    // A whole number from low to high, both included.
    int nextInt(int low, int high)
    {
        return low + static_cast<int>(next() % static_cast<unsigned int>(high - low + 1));
    }

    // A number from low up to (almost) high.
    double nextDouble(double low, double high)
    {
        return low + (high - low) * (static_cast<double>(next()) / 2147483648.0);
    }

private:
    unsigned long long state;
};

static ScoringWeights makeWeights(double preference, double gpa, double statement, double fairness)
{
    ScoringWeights w;
    w.preference = preference;
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

// A university with no restrictions (accepts every major).
static AdminUniversity makeUniversity(const std::string &slug)
{
    AdminUniversity u;
    u.slug = slug;
    u.name = slug;
    u.acceptedMajors.push_back("ALL");
    return u;
}

// A student with no choices yet. Callers fill in prefs[] themselves.
static AdminStudent makeStudent(const std::string &name, double gpa, double statement)
{
    AdminStudent s;
    s.id = 1;
    s.name = name;
    s.gpa = gpa;
    s.majorCode = "CSCE";
    s.semester = "Fall";
    s.statementRating = statement;
    s.hasStatementRating = true;
    return s;
}

// A student whose first choice is the given university.
static AdminStudent makeStudentWithFirstChoice(const std::string &name, double gpa, double statement,
                                               const std::string &slug)
{
    AdminStudent s = makeStudent(name, gpa, statement);
    s.prefs[0] = slug;
    return s;
}

// One admin seat-count entry.
static AdminSeatCount makeCount(const std::string &slug, int seats)
{
    AdminSeatCount c;
    c.slug = slug;
    c.seats = seats;
    return c;
}

// Position of a university in a result, or -1 when it is not there.
static int findUniversity(const AdminMatchResult &result, const std::string &slug)
{
    for (int u = 0; u < result.byUniversity.size(); ++u)
    {
        if (result.byUniversity[u].slug == slug)
            return u;
    }
    return -1;
}

// Position of a student (by name) among those placed at one university, or -1.
static int findPlaced(const AdminUniversityResult &university, const std::string &name)
{
    for (int i = 0; i < university.placed.size(); ++i)
    {
        if (university.placed[i].student.name == name)
            return i;
    }
    return -1;
}

// True when a student (by name) is in the unplaced list.
static bool isUnplaced(const AdminMatchResult &result, const std::string &name)
{
    for (int i = 0; i < result.unplaced.size(); ++i)
    {
        if (result.unplaced[i].name == name)
            return true;
    }
    return false;
}

// How many students got a seat, over all universities.
static int countPlaced(const AdminMatchResult &result)
{
    int total = 0;
    for (int u = 0; u < result.byUniversity.size(); ++u)
        total += result.byUniversity[u].placed.size();
    return total;
}

// The sum of the scores of everyone who got a seat.
static double totalScore(const AdminMatchResult &result)
{
    double total = 0.0;
    for (int u = 0; u < result.byUniversity.size(); ++u)
    {
        for (int i = 0; i < result.byUniversity[u].placed.size(); ++i)
            total += result.byUniversity[u].placed[i].score;
    }
    return total;
}

// True when two results say exactly the same thing (same people, seats and scores).
static bool sameResult(const AdminMatchResult &a, const AdminMatchResult &b)
{
    if (a.valid != b.valid || a.byUniversity.size() != b.byUniversity.size() ||
        a.unplaced.size() != b.unplaced.size())
        return false;

    for (int u = 0; u < a.byUniversity.size(); ++u)
    {
        const AdminUniversityResult &x = a.byUniversity[u];
        const AdminUniversityResult &y = b.byUniversity[u];
        if (x.slug != y.slug || x.name != y.name || x.totalSeats != y.totalSeats ||
            x.seatsAssumed != y.seatsAssumed || x.placed.size() != y.placed.size())
            return false;
        for (int i = 0; i < x.placed.size(); ++i)
        {
            if (x.placed[i].student.name != y.placed[i].student.name ||
                x.placed[i].seatLabel != y.placed[i].seatLabel ||
                !near(x.placed[i].score, y.placed[i].score, 1e-12))
                return false;
        }
    }

    for (int i = 0; i < a.unplaced.size(); ++i)
    {
        if (a.unplaced[i].name != b.unplaced[i].name)
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// The usual example, shared by several tests
// ---------------------------------------------------------------------------

// Youssef, Laila and Salma (in that order) want uva as 1st, 2nd and 3rd choice.
// Universities: uva, tum, sciences-po. The admin set uva = 2 and sciences-po = 0.
static void fillUsualExample(Vector<AdminStudent> &students,
                             Vector<AdminUniversity> &universities,
                             Vector<AdminSeatCount> &counts)
{
    AdminStudent youssef = makeStudent("Youssef", 3.78, 8.9);
    youssef.prefs[0] = "uva";
    AdminStudent laila = makeStudent("Laila", 3.85, 9.0);
    laila.prefs[1] = "uva";
    AdminStudent salma = makeStudent("Salma", 3.92, 9.2);
    salma.prefs[2] = "uva";
    students.push_back(youssef);
    students.push_back(laila);
    students.push_back(salma);

    universities.push_back(makeUniversity("uva"));
    universities.push_back(makeUniversity("tum"));
    universities.push_back(makeUniversity("sciences-po"));

    counts.push_back(makeCount("uva", 2));
    counts.push_back(makeCount("sciences-po", 0));
}

// Checks every fact about the usual example's result. The label names the run.
static void checkUsualResult(const AdminMatchResult &result, const std::string &label)
{
    check(result.valid, label + ": result is valid");
    check(result.byUniversity.size() == 3, label + ": 3 universities in the result");
    if (result.byUniversity.size() != 3)
        return;

    check(result.byUniversity[0].slug == "uva" && result.byUniversity[1].slug == "tum" &&
              result.byUniversity[2].slug == "sciences-po",
          label + ": universities are in the original order");

    const AdminUniversityResult &uva = result.byUniversity[0];
    check(uva.totalSeats == 2, label + ": uva has 2 seats");
    check(!uva.seatsAssumed, label + ": uva seats were set by the admin");
    check(uva.placed.size() == 2, label + ": 2 students placed at uva");

    int youssef = findPlaced(uva, "Youssef");
    int laila = findPlaced(uva, "Laila");
    check(youssef >= 0, label + ": Youssef is placed at uva");
    check(laila >= 0, label + ": Laila is placed at uva");
    if (youssef >= 0)
    {
        check(uva.placed[youssef].seatLabel == "uva #1", label + ": Youssef is at 'uva #1'");
        check(near(uva.placed[youssef].score, 0.93, 0.005), label + ": Youssef's score is about 0.93");
    }
    if (laila >= 0)
    {
        check(uva.placed[laila].seatLabel == "uva #2", label + ": Laila is at 'uva #2'");
        check(near(uva.placed[laila].score, 0.83, 0.005), label + ": Laila's score is about 0.83");
    }

    check(result.unplaced.size() == 1, label + ": exactly 1 student unplaced");
    check(isUnplaced(result, "Salma"), label + ": Salma is the unplaced student");

    const AdminUniversityResult &tum = result.byUniversity[1];
    check(tum.placed.size() == 0, label + ": nobody placed at tum");
    check(tum.totalSeats == DEFAULT_SEATS_PER_UNIVERSITY, label + ": tum has the default seats");
    check(tum.seatsAssumed, label + ": tum seats are assumed");

    const AdminUniversityResult &sciencesPo = result.byUniversity[2];
    check(sciencesPo.totalSeats == 0, label + ": sciences-po has 0 seats");
    check(sciencesPo.placed.size() == 0, label + ": nobody placed at sciences-po");
}

// ---------------------------------------------------------------------------
// a) The usual example
// ---------------------------------------------------------------------------

static void testUsualExample()
{
    Vector<AdminStudent> students;
    Vector<AdminUniversity> universities;
    Vector<AdminSeatCount> counts;
    fillUsualExample(students, universities, counts);

    AdminAllocator allocator(students, universities, counts, false, defaultWeights());
    check(allocator.valid(), "usual example: allocator is valid");

    // Note: runMostPlaced ignores scores, so which students get the seats follows the
    // order the students were added (Youssef, Laila, Salma). The check for runMostPlaced
    // below depends on that order. runBestFit picks by score, so it does not.
    checkUsualResult(allocator.runMostPlaced(), "usual example, most placed");
    checkUsualResult(allocator.runBestFit(), "usual example, best fit");
}

// ---------------------------------------------------------------------------
// b) The case plain Hungarian gets wrong
// ---------------------------------------------------------------------------

static void checkBothPlaced(const AdminMatchResult &result, const std::string &label)
{
    check(result.valid, label + ": valid");
    check(result.unplaced.size() == 0, label + ": nobody unplaced");
    int x = findUniversity(result, "X");
    int y = findUniversity(result, "Y");
    check(x >= 0 && y >= 0, label + ": both universities are in the result");
    if (x < 0 || y < 0)
        return;
    check(result.byUniversity[x].placed.size() == 1 && findPlaced(result.byUniversity[x], "A") == 0,
          label + ": A is placed at X");
    check(result.byUniversity[y].placed.size() == 1 && findPlaced(result.byUniversity[y], "B") == 0,
          label + ": B is placed at Y");
}

static void testBothMustBePlaced()
{
    // A only has X (third choice). B has X (first choice) and Y (third choice).
    // The only way to place both is A at X and B at Y.
    AdminStudent a = makeStudent("A", 3.5, 8.0);
    a.prefs[2] = "X";
    AdminStudent b = makeStudent("B", 3.5, 8.0);
    b.prefs[0] = "X";
    b.prefs[2] = "Y";

    Vector<AdminStudent> students;
    students.push_back(a);
    students.push_back(b);

    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("X"));
    universities.push_back(makeUniversity("Y"));

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("X", 1));
    counts.push_back(makeCount("Y", 1));

    AdminAllocator allocator(students, universities, counts, false, makeWeights(1.0, 0.0, 0.0, 0.0));
    check(allocator.valid(), "both placed: allocator is valid");
    checkBothPlaced(allocator.runMostPlaced(), "both placed, most placed");
    checkBothPlaced(allocator.runBestFit(), "both placed, best fit");
}

// ---------------------------------------------------------------------------
// c) runBestFit picks the better total among equal counts
// ---------------------------------------------------------------------------

static void testBestFitPicksHigherScore()
{
    // One seat, two eligible students. The weaker one is listed first.
    Vector<AdminStudent> students;
    students.push_back(makeStudentWithFirstChoice("Weak", 3.0, 6.0, "uva"));
    students.push_back(makeStudentWithFirstChoice("Strong", 3.9, 9.5, "uva"));

    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 1));

    AdminAllocator allocator(students, universities, counts, false, defaultWeights());
    AdminMatchResult result = allocator.runBestFit();

    check(result.valid, "best fit tie-break: valid");
    check(countPlaced(result) == 1, "best fit tie-break: one student placed");
    check(result.byUniversity.size() == 1 && findPlaced(result.byUniversity[0], "Strong") >= 0,
          "best fit tie-break: the higher-scoring student is placed");
    check(isUnplaced(result, "Weak"), "best fit tie-break: the lower-scoring student is unplaced");
}

// ---------------------------------------------------------------------------
// d) Capacity
// ---------------------------------------------------------------------------

static void checkCapacity(const AdminMatchResult &result, const std::string &label)
{
    check(result.valid, label + ": valid");
    check(countPlaced(result) == 3, label + ": exactly 3 placed");
    check(result.unplaced.size() == 2, label + ": the other 2 are unplaced");
    check(result.byUniversity.size() == 1 && result.byUniversity[0].totalSeats == 3 &&
              result.byUniversity[0].placed.size() == 3,
          label + ": uva is full with 3");
}

static void testCapacity()
{
    Vector<AdminStudent> students;
    for (int i = 0; i < 5; ++i)
        students.push_back(makeStudentWithFirstChoice("S" + std::to_string(i), 3.0 + 0.1 * i, 7.0 + 0.2 * i, "uva"));

    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 3));

    AdminAllocator allocator(students, universities, counts, false, defaultWeights());
    checkCapacity(allocator.runMostPlaced(), "capacity, most placed");
    checkCapacity(allocator.runBestFit(), "capacity, best fit");
}

// ---------------------------------------------------------------------------
// e) A student with no eligible university
// ---------------------------------------------------------------------------

static void checkNoEligible(const AdminMatchResult &result, const std::string &label)
{
    check(result.valid, label + ": valid");
    check(isUnplaced(result, "LowGpa"), label + ": the low-GPA student is unplaced");
    check(countPlaced(result) == 1, label + ": the good student is placed");
    check(countPlaced(result) + result.unplaced.size() == 2, label + ": placed + unplaced = students");
}

static void testNoEligibleUniversity()
{
    Vector<AdminStudent> students;
    students.push_back(makeStudentWithFirstChoice("LowGpa", 2.5, 8.0, "uva"));  // below the AUC minimum
    students.push_back(makeStudentWithFirstChoice("Good", 3.5, 8.0, "uva"));

    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 2));

    AdminAllocator allocator(students, universities, counts, false, defaultWeights());
    checkNoEligible(allocator.runMostPlaced(), "no eligible, most placed");
    checkNoEligible(allocator.runBestFit(), "no eligible, best fit");
}

// ---------------------------------------------------------------------------
// f) allowUnranked
// ---------------------------------------------------------------------------

static void testAllowUnranked()
{
    // Both students only want uva, which has 1 seat. tum has 1 free seat.
    Vector<AdminStudent> students;
    students.push_back(makeStudentWithFirstChoice("First", 3.6, 8.0, "uva"));
    students.push_back(makeStudentWithFirstChoice("Second", 3.5, 8.0, "uva"));

    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));
    universities.push_back(makeUniversity("tum"));

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 1));
    counts.push_back(makeCount("tum", 1));

    AdminAllocator strict(students, universities, counts, false, defaultWeights());
    AdminAllocator relaxed(students, universities, counts, true, defaultWeights());

    for (int run = 0; run < 2; ++run)
    {
        std::string label = run == 0 ? "unranked, most placed" : "unranked, best fit";
        AdminMatchResult noUnranked = run == 0 ? strict.runMostPlaced() : strict.runBestFit();
        AdminMatchResult withUnranked = run == 0 ? relaxed.runMostPlaced() : relaxed.runBestFit();

        check(noUnranked.valid && withUnranked.valid, label + ": both valid");

        // allowUnranked false: the student who misses uva stays unplaced.
        check(countPlaced(noUnranked) == 1, label + ": false, only 1 placed");
        check(noUnranked.unplaced.size() == 1, label + ": false, 1 stays unplaced");
        int tum = findUniversity(noUnranked, "tum");
        check(tum >= 0 && noUnranked.byUniversity[tum].placed.size() == 0,
              label + ": false, tum stays empty");

        // allowUnranked true: the other student gets the free seat at tum.
        check(countPlaced(withUnranked) == 2, label + ": true, both placed");
        check(withUnranked.unplaced.size() == 0, label + ": true, nobody unplaced");
        tum = findUniversity(withUnranked, "tum");
        int uva = findUniversity(withUnranked, "uva");
        check(tum >= 0 && withUnranked.byUniversity[tum].placed.size() == 1,
              label + ": true, tum has one student");
        check(uva >= 0 && withUnranked.byUniversity[uva].placed.size() == 1,
              label + ": true, uva has one student");
    }
}

// ---------------------------------------------------------------------------
// g) Invalid inputs
// ---------------------------------------------------------------------------

static void testInvalidInputs()
{
    Vector<AdminStudent> students;
    Vector<AdminUniversity> universities;
    Vector<AdminSeatCount> counts;
    fillUsualExample(students, universities, counts);

    // All-zero weights.
    AdminAllocator zero(students, universities, counts, false, makeWeights(0.0, 0.0, 0.0, 0.0));
    check(!zero.valid(), "invalid weights: valid() is false");
    AdminMatchResult zeroMost = zero.runMostPlaced();
    AdminMatchResult zeroBest = zero.runBestFit();
    check(!zeroMost.valid && zeroMost.byUniversity.size() == 0, "invalid weights: most placed is invalid and empty");
    check(!zeroBest.valid && zeroBest.byUniversity.size() == 0, "invalid weights: best fit is invalid and empty");

    // A later valid setWeights fixes it.
    check(zero.setWeights(defaultWeights()), "invalid weights: setWeights with valid weights returns true");
    check(zero.valid(), "invalid weights: valid() is true after valid weights");
    checkUsualResult(zero.runMostPlaced(), "after fixing weights, most placed");
    checkUsualResult(zero.runBestFit(), "after fixing weights, best fit");

    // A negative admin seat count.
    Vector<AdminSeatCount> negativeCounts;
    negativeCounts.push_back(makeCount("uva", -1));
    AdminAllocator negative(students, universities, negativeCounts, false, defaultWeights());
    check(!negative.valid(), "negative seats: valid() is false");
    AdminMatchResult negMost = negative.runMostPlaced();
    AdminMatchResult negBest = negative.runBestFit();
    check(!negMost.valid && negMost.byUniversity.size() == 0, "negative seats: most placed is invalid and empty");
    check(!negBest.valid && negBest.byUniversity.size() == 0, "negative seats: best fit is invalid and empty");
    negative.setWeights(defaultWeights());
    check(!negative.valid(), "negative seats: still invalid after setting weights");

    // setWeights with invalid weights changes nothing.
    AdminAllocator allocator(students, universities, counts, false, defaultWeights());
    AdminMatchResult beforeBest = allocator.runBestFit();
    AdminMatchResult beforeMost = allocator.runMostPlaced();
    check(beforeBest.valid && countPlaced(beforeBest) == 2, "unchanged scores: the starting result is real");

    check(!allocator.setWeights(makeWeights(0.0, 0.0, 0.0, 0.0)), "unchanged scores: all-zero weights refused");
    check(!allocator.setWeights(makeWeights(1.0, -0.5, 0.0, 0.0)), "unchanged scores: negative weight refused");
    check(allocator.valid(), "unchanged scores: allocator is still valid");
    check(sameResult(beforeBest, allocator.runBestFit()), "unchanged scores: best fit result is the same");
    check(sameResult(beforeMost, allocator.runMostPlaced()), "unchanged scores: most placed result is the same");
}

// ---------------------------------------------------------------------------
// h) Moving the sliders changes the scores
// ---------------------------------------------------------------------------

static void testSlidersChangeScores()
{
    Vector<AdminStudent> students;
    students.push_back(makeStudentWithFirstChoice("Solo", 3.0, 9.0, "uva"));
    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));
    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 1));

    AdminAllocator allocator(students, universities, counts, false, makeWeights(1.0, 1.0, 1.0, 1.0));
    AdminMatchResult before = allocator.runBestFit();
    check(allocator.setWeights(makeWeights(0.0, 1.0, 0.0, 0.0)), "sliders: setWeights accepted");
    AdminMatchResult after = allocator.runBestFit();

    check(before.valid && after.valid, "sliders: both runs valid");
    check(countPlaced(before) == 1 && countPlaced(after) == 1, "sliders: the student is placed both times");
    if (countPlaced(before) == 1 && countPlaced(after) == 1)
    {
        double scoreBefore = before.byUniversity[0].placed[0].score;
        double scoreAfter = after.byUniversity[0].placed[0].score;
        check(!near(scoreBefore, scoreAfter, 1e-6), "sliders: the score changed with the weights");
    }

    // The most-placed option reports the new scores too.
    AdminMatchResult most = allocator.runMostPlaced();
    check(countPlaced(most) == 1 && near(most.byUniversity[0].placed[0].score,
                                         after.byUniversity[0].placed[0].score, 1e-12),
          "sliders: most placed shows the same new score");
}

// ---------------------------------------------------------------------------
// i) The result is a snapshot
// ---------------------------------------------------------------------------

static void testSnapshot()
{
    // The allocator and its input lists are destroyed at the end of the block.
    AdminMatchResult kept;
    {
        Vector<AdminStudent> students;
        Vector<AdminUniversity> universities;
        Vector<AdminSeatCount> counts;
        fillUsualExample(students, universities, counts);
        AdminAllocator allocator(students, universities, counts, false, defaultWeights());
        kept = allocator.runBestFit();
    }
    checkUsualResult(kept, "snapshot after lists are gone");

    // Changing the original lists after the allocator was built changes nothing.
    Vector<AdminStudent> students;
    Vector<AdminUniversity> universities;
    Vector<AdminSeatCount> counts;
    fillUsualExample(students, universities, counts);
    AdminAllocator allocator(students, universities, counts, false, defaultWeights());
    AdminMatchResult before = allocator.runBestFit();

    students[0].name = "Changed";
    students[0].gpa = 1.0;
    students.push_back(makeStudentWithFirstChoice("Extra", 3.9, 9.9, "uva"));
    universities[0].slug = "renamed";
    counts[0].seats = 0;

    AdminMatchResult after = allocator.runBestFit();
    check(sameResult(before, after), "snapshot: a later run ignores changes to the original lists");
    checkUsualResult(after, "snapshot: run after the lists changed");
}

// ---------------------------------------------------------------------------
// j) Running the options on the same allocator
// ---------------------------------------------------------------------------

static void testRunsDoNotInterfere()
{
    Vector<AdminStudent> students;
    Vector<AdminUniversity> universities;
    Vector<AdminSeatCount> counts;
    fillUsualExample(students, universities, counts);
    AdminAllocator allocator(students, universities, counts, false, defaultWeights());

    AdminMatchResult first = allocator.runMostPlaced();
    AdminMatchResult best = allocator.runBestFit();
    check(best.valid, "same allocator: best fit is valid");
    check(sameResult(first, allocator.runMostPlaced()),
          "same allocator: the first result is unchanged after running the other option");

    AdminMatchResult bestAgain = allocator.runBestFit();
    check(sameResult(best, bestAgain), "same allocator: best fit twice gives the same result");
    AdminMatchResult mostAgain = allocator.runMostPlaced();
    check(sameResult(first, mostAgain), "same allocator: most placed twice gives the same result");
}

// ---------------------------------------------------------------------------
// k) Edge cases
// ---------------------------------------------------------------------------

static void checkNoStudents(const AdminMatchResult &result, const std::string &label)
{
    check(result.valid, label + ": valid");
    check(result.unplaced.size() == 0, label + ": nobody unplaced");
    check(result.byUniversity.size() == 3, label + ": all universities present");
    check(countPlaced(result) == 0, label + ": nobody placed");
}

static void checkNoUniversities(const AdminMatchResult &result, const std::string &label)
{
    check(result.valid, label + ": valid");
    check(result.byUniversity.size() == 0, label + ": no universities in the result");
    check(result.unplaced.size() == 2, label + ": every student is unplaced");
}

static void testEdgeCases()
{
    // No students.
    Vector<AdminStudent> noStudents;
    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));
    universities.push_back(makeUniversity("tum"));
    universities.push_back(makeUniversity("sciences-po"));
    AdminAllocator empty(noStudents, universities, Vector<AdminSeatCount>(), false, defaultWeights());
    check(empty.valid(), "no students: allocator is valid");
    checkNoStudents(empty.runMostPlaced(), "no students, most placed");
    checkNoStudents(empty.runBestFit(), "no students, best fit");

    // No universities.
    Vector<AdminStudent> students;
    students.push_back(makeStudentWithFirstChoice("One", 3.5, 8.0, "uva"));
    students.push_back(makeStudentWithFirstChoice("Two", 3.6, 8.5, "tum"));
    AdminAllocator lonely(students, Vector<AdminUniversity>(), Vector<AdminSeatCount>(), false, defaultWeights());
    check(lonely.valid(), "no universities: allocator is valid");
    checkNoUniversities(lonely.runMostPlaced(), "no universities, most placed");
    checkNoUniversities(lonely.runBestFit(), "no universities, best fit");
}

// ---------------------------------------------------------------------------
// l) Random comparison against brute force
// ---------------------------------------------------------------------------

// Tries every way of placing the students (each one skips, or takes a free eligible
// seat) and remembers the best number placed, and the best total score for that number.
struct BruteForce
{
    const Vector<AdminSeatEdge> &edges;
    int studentCount;
    Vector<bool> used;   // which seats are taken right now
    int bestCount;
    double bestTotal;

    BruteForce(const Vector<AdminSeatEdge> &edgeList, int students, int seats)
        : edges(edgeList), studentCount(students), used(seats, false), bestCount(0), bestTotal(0.0)
    {
    }

    void go(int student, int count, double total)
    {
        if (student == studentCount)
        {
            if (count > bestCount || (count == bestCount && total > bestTotal))
            {
                bestCount = count;
                bestTotal = total;
            }
            return;
        }

        go(student + 1, count, total);  // this student takes no seat

        for (int i = 0; i < edges.size(); ++i)
        {
            if (edges[i].studentIndex != student || used[edges[i].seatIndex])
                continue;
            used[edges[i].seatIndex] = true;
            go(student + 1, count + 1, total + edges[i].score);
            used[edges[i].seatIndex] = false;
        }
    }
};

static void checkRandomRun(const AdminMatchResult &result, const AdminSeatTable &table,
                           int studentCount, const std::string &label)
{
    check(result.valid, label + ": valid");
    check(countPlaced(result) + result.unplaced.size() == studentCount,
          label + ": placed + unplaced equals the student count");
    for (int u = 0; u < result.byUniversity.size(); ++u)
    {
        check(result.byUniversity[u].placed.size() <= result.byUniversity[u].totalSeats,
              label + ": no university is over its seats");
        check(result.byUniversity[u].totalSeats == table.universitySeats(u).totalSeats,
              label + ": totalSeats matches the seat table");
    }
}

static void testRandomAgainstBruteForce()
{
    Lcg rng(20240607ULL);

    for (int trial = 0; trial < 500; ++trial)
    {
        int failuresBefore = failures;
        std::string tag = "random trial " + std::to_string(trial);

        int studentCount = rng.nextInt(1, 6);
        int universityCount = rng.nextInt(1, 4);
        int defaultSeats = rng.nextInt(0, 2);

        // Universities, and admin seat counts for about half of them.
        Vector<AdminUniversity> universities;
        Vector<AdminSeatCount> counts;
        for (int u = 0; u < universityCount; ++u)
        {
            std::string slug = "u" + std::to_string(u);
            AdminUniversity university = makeUniversity(slug);
            if (rng.nextInt(0, 2) == 0)  // about a third know their minimum GPA
            {
                university.minGpaKnown = true;
                university.minGpa = rng.nextDouble(2.8, 3.9);
            }
            universities.push_back(university);
            if (rng.nextInt(0, 1) == 1)
                counts.push_back(makeCount(slug, rng.nextInt(0, 2)));
        }

        // Students with random GPA, choices (some empty), statement and history.
        Vector<AdminStudent> students;
        for (int s = 0; s < studentCount; ++s)
        {
            AdminStudent student = makeStudent("S" + std::to_string(s),
                                               rng.nextDouble(2.6, 4.0),
                                               rng.nextDouble(3.0, 10.0));
            Vector<bool> taken(universityCount, false);
            for (int rank = 0; rank < 3; ++rank)
            {
                if (rng.nextInt(0, 3) == 0)
                    continue;  // no choice for this rank
                int pick = rng.nextInt(0, universityCount - 1);
                if (taken[pick])
                    continue;  // a university is never ranked twice
                taken[pick] = true;
                student.prefs[rank] = "u" + std::to_string(pick);
            }
            student.studiedAbroadBefore = rng.nextInt(0, 1) == 1;
            if (rng.nextInt(0, 7) == 0)  // about one in eight has no statement rating yet
                student.hasStatementRating = false;
            students.push_back(student);
        }

        bool allowUnranked = rng.nextInt(0, 1) == 1;
        ScoringWeights weights = makeWeights(0.1 + 0.9 * rng.nextDouble(0.0, 1.0),
                                             rng.nextDouble(0.0, 1.0),
                                             rng.nextDouble(0.0, 1.0),
                                             rng.nextDouble(0.0, 1.0));

        AdminAllocator allocator(students, universities, counts, allowUnranked, weights, defaultSeats);

        // The same inputs again, built separately, to get the lines for the brute force.
        AdminFitScorer scorer(students, universities, allowUnranked, weights);
        AdminSeatTable table(universities, counts, defaultSeats);
        Vector<AdminSeatEdge> edges = table.buildEdges(scorer.pairs());

        BruteForce brute(edges, studentCount, table.seatCount());
        brute.go(0, 0, 0.0);

        AdminMatchResult most = allocator.runMostPlaced();
        AdminMatchResult best = allocator.runBestFit();

        checkRandomRun(most, table, studentCount, tag + ", most placed");
        checkRandomRun(best, table, studentCount, tag + ", best fit");
        check(countPlaced(most) == brute.bestCount, tag + ": most placed reaches the best possible count");
        check(countPlaced(best) == brute.bestCount, tag + ": best fit reaches the best possible count");
        check(near(totalScore(best), brute.bestTotal, 1e-9), tag + ": best fit reaches the best total score");

        if (failures != failuresBefore)
        {
            std::cout << "FAIL: random trial " << trial << " of 500 had failing checks (see above); stopping\n";
            return;
        }
    }
}

int main()
{
    testUsualExample();
    testBothMustBePlaced();
    testBestFitPicksHigherScore();
    testCapacity();
    testNoEligibleUniversity();
    testAllowUnranked();
    testInvalidInputs();
    testSlidersChangeScores();
    testSnapshot();
    testRunsDoNotInterfere();
    testEdgeCases();
    testRandomAgainstBruteForce();

    if (failures == 0)
        std::cout << "All admin allocator checks passed\n";
    else
        std::cout << failures << " check(s) failed\n";
    return failures == 0 ? 0 : 1;
}
