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

// ---------------------------------------------------------------------------
// Option 3 (runTopCandidates): helpers
// ---------------------------------------------------------------------------

// True when two shortlist results say exactly the same thing.
static bool sameShortlistResult(const AdminShortlistResult &a, const AdminShortlistResult &b)
{
    if (a.valid != b.valid || a.byUniversity.size() != b.byUniversity.size())
        return false;

    for (int u = 0; u < a.byUniversity.size(); ++u)
    {
        const AdminUniversityShortlist &x = a.byUniversity[u];
        const AdminUniversityShortlist &y = b.byUniversity[u];
        if (x.slug != y.slug || x.name != y.name || x.totalSeats != y.totalSeats ||
            x.seatsAssumed != y.seatsAssumed || x.eligibleCount != y.eligibleCount ||
            x.candidates.size() != y.candidates.size())
            return false;
        for (int i = 0; i < x.candidates.size(); ++i)
        {
            if (x.candidates[i].rank != y.candidates[i].rank ||
                x.candidates[i].student.name != y.candidates[i].student.name ||
                !near(x.candidates[i].score, y.candidates[i].score, 1e-12) ||
                x.candidates[i].minGpaUnknown != y.candidates[i].minGpaUnknown ||
                x.candidates[i].statementMissing != y.candidates[i].statementMissing)
                return false;
        }
    }
    return true;
}

// True when the ranks are exactly 1, 2, 3, ... and the scores never go up down the list.
static bool listIsOrdered(const AdminUniversityShortlist &shortlist)
{
    for (int i = 0; i < shortlist.candidates.size(); ++i)
    {
        if (shortlist.candidates[i].rank != i + 1)
            return false;
        if (i > 0 && shortlist.candidates[i].score > shortlist.candidates[i - 1].score)
            return false;
    }
    return true;
}

// Checks every fact about the usual example's shortlist (no cap). The label names the run.
static void checkUsualShortlist(const AdminShortlistResult &result, const std::string &label)
{
    check(result.valid, label + ": result is valid");
    check(result.byUniversity.size() == 3, label + ": 3 universities in the result");
    if (result.byUniversity.size() != 3)
        return;

    check(result.byUniversity[0].slug == "uva" && result.byUniversity[1].slug == "tum" &&
              result.byUniversity[2].slug == "sciences-po",
          label + ": universities are in the original order");

    const AdminUniversityShortlist &uva = result.byUniversity[0];
    check(uva.totalSeats == 2, label + ": uva has 2 seats");
    check(!uva.seatsAssumed, label + ": uva seats were set by the admin");
    check(uva.eligibleCount == 3, label + ": 3 students are eligible at uva");
    check(uva.candidates.size() == 3, label + ": 3 candidates listed at uva (seats do not cut the list)");
    if (uva.candidates.size() == 3)
    {
        check(uva.candidates[0].student.name == "Youssef" && uva.candidates[0].rank == 1,
              label + ": Youssef is rank 1 at uva");
        check(uva.candidates[1].student.name == "Laila" && uva.candidates[1].rank == 2,
              label + ": Laila is rank 2 at uva");
        check(uva.candidates[2].student.name == "Salma" && uva.candidates[2].rank == 3,
              label + ": Salma is rank 3 at uva");
        check(near(uva.candidates[0].score, 0.93, 0.005), label + ": Youssef's score is about 0.93");
        check(near(uva.candidates[1].score, 0.83, 0.005), label + ": Laila's score is about 0.83");
        check(near(uva.candidates[2].score, 0.73, 0.005), label + ": Salma's score is about 0.73");
    }

    const AdminUniversityShortlist &tum = result.byUniversity[1];
    check(tum.candidates.size() == 0, label + ": nobody listed at tum");
    check(tum.eligibleCount == 0, label + ": nobody eligible at tum");
    check(tum.seatsAssumed, label + ": tum seats are assumed");
    check(tum.totalSeats == DEFAULT_SEATS_PER_UNIVERSITY, label + ": tum has the default seats");

    const AdminUniversityShortlist &sciencesPo = result.byUniversity[2];
    check(sciencesPo.totalSeats == 0, label + ": sciences-po has 0 seats");
    check(sciencesPo.candidates.size() == 0, label + ": nobody listed at sciences-po");
}

// ---------------------------------------------------------------------------
// m) Option 3: the usual example
// ---------------------------------------------------------------------------

static void testShortlistUsualExample()
{
    Vector<AdminStudent> students;
    Vector<AdminUniversity> universities;
    Vector<AdminSeatCount> counts;
    fillUsualExample(students, universities, counts);

    AdminAllocator allocator(students, universities, counts, false, defaultWeights());
    check(allocator.valid(), "shortlist usual example: allocator is valid");
    checkUsualShortlist(allocator.runTopCandidates(0), "shortlist usual example");
}

// ---------------------------------------------------------------------------
// n) Option 3: the cap
// ---------------------------------------------------------------------------

static void testShortlistCap()
{
    Vector<AdminStudent> students;
    Vector<AdminUniversity> universities;
    Vector<AdminSeatCount> counts;
    fillUsualExample(students, universities, counts);
    AdminAllocator allocator(students, universities, counts, false, defaultWeights());

    AdminShortlistResult capped = allocator.runTopCandidates(2);
    check(capped.valid && capped.byUniversity.size() == 3, "shortlist cap 2: valid, 3 universities");
    if (capped.valid && capped.byUniversity.size() == 3)
    {
        const AdminUniversityShortlist &uva = capped.byUniversity[0];
        check(uva.candidates.size() == 2, "shortlist cap 2: only 2 listed at uva");
        check(uva.eligibleCount == 3, "shortlist cap 2: eligibleCount stays 3");
        if (uva.candidates.size() == 2)
        {
            check(uva.candidates[0].student.name == "Youssef" && uva.candidates[0].rank == 1,
                  "shortlist cap 2: Youssef is rank 1");
            check(uva.candidates[1].student.name == "Laila" && uva.candidates[1].rank == 2,
                  "shortlist cap 2: Laila is rank 2");
        }
        check(capped.byUniversity[1].candidates.size() == 0, "shortlist cap 2: tum still has nobody");
    }

    // 0 and negative caps both mean everyone.
    checkUsualShortlist(allocator.runTopCandidates(0), "shortlist cap 0");
    checkUsualShortlist(allocator.runTopCandidates(-5), "shortlist cap -5");

    // A cap bigger than the number of eligible students changes nothing.
    checkUsualShortlist(allocator.runTopCandidates(10), "shortlist cap 10");
}

// ---------------------------------------------------------------------------
// o) Option 3: allowUnranked
// ---------------------------------------------------------------------------

static void testShortlistAllowUnranked()
{
    Vector<AdminStudent> students;
    Vector<AdminUniversity> universities;
    Vector<AdminSeatCount> counts;
    fillUsualExample(students, universities, counts);
    AdminAllocator allocator(students, universities, counts, true, defaultWeights());

    AdminShortlistResult result = allocator.runTopCandidates(0);
    check(result.valid && result.byUniversity.size() == 3, "shortlist unranked: valid, 3 universities");
    if (!result.valid || result.byUniversity.size() != 3)
        return;

    for (int u = 0; u < 3; ++u)
    {
        const AdminUniversityShortlist &shortlist = result.byUniversity[u];
        std::string label = "shortlist unranked, " + shortlist.slug;
        check(shortlist.eligibleCount == 3, label + ": 3 eligible");
        check(shortlist.candidates.size() == 3, label + ": 3 listed");
        check(listIsOrdered(shortlist), label + ": ranks are 1, 2, 3 and scores never go up");
    }
}

// ---------------------------------------------------------------------------
// p) Option 3: ties are listed in student order
// ---------------------------------------------------------------------------

static void testShortlistTies()
{
    Vector<AdminStudent> students;
    students.push_back(makeStudentWithFirstChoice("A", 3.5, 8.0, "uva"));
    students.push_back(makeStudentWithFirstChoice("B", 3.5, 8.0, "uva"));
    students.push_back(makeStudentWithFirstChoice("C", 3.5, 8.0, "uva"));
    students.push_back(makeStudentWithFirstChoice("D", 3.5, 8.0, "uva"));

    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));
    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 2));

    AdminAllocator allocator(students, universities, counts, false, defaultWeights());
    AdminShortlistResult result = allocator.runTopCandidates(0);
    check(result.valid && result.byUniversity.size() == 1, "shortlist ties: valid, 1 university");
    if (!result.valid || result.byUniversity.size() != 1)
        return;

    const AdminUniversityShortlist &uva = result.byUniversity[0];
    check(uva.eligibleCount == 4 && uva.candidates.size() == 4, "shortlist ties: all 4 listed");
    if (uva.candidates.size() != 4)
        return;
    check(uva.candidates[0].student.name == "A" && uva.candidates[1].student.name == "B" &&
              uva.candidates[2].student.name == "C" && uva.candidates[3].student.name == "D",
          "shortlist ties: equal scores are listed in student order");
    check(near(uva.candidates[0].score, uva.candidates[3].score, 1e-12), "shortlist ties: the scores really are equal");
    check(listIsOrdered(uva), "shortlist ties: ranks are 1, 2, 3, 4");
}

// ---------------------------------------------------------------------------
// q) Option 3: warning flags
// ---------------------------------------------------------------------------

static void testShortlistFlags()
{
    // "known" has a known minimum GPA. "unknown" does not (the default).
    AdminUniversity known = makeUniversity("known");
    known.minGpaKnown = true;
    known.minGpa = 3.0;
    AdminUniversity unknown = makeUniversity("unknown");

    Vector<AdminUniversity> universities;
    universities.push_back(known);
    universities.push_back(unknown);

    AdminStudent normal = makeStudentWithFirstChoice("Normal", 3.5, 8.0, "known");
    AdminStudent noStatement = makeStudentWithFirstChoice("NoStatement", 3.4, 8.0, "known");
    noStatement.hasStatementRating = false;
    AdminStudent atUnknown = makeStudentWithFirstChoice("AtUnknown", 3.6, 8.0, "unknown");

    Vector<AdminStudent> students;
    students.push_back(normal);
    students.push_back(noStatement);
    students.push_back(atUnknown);

    AdminAllocator allocator(students, universities, Vector<AdminSeatCount>(), false, defaultWeights());
    AdminShortlistResult result = allocator.runTopCandidates(0);
    check(result.valid && result.byUniversity.size() == 2, "flags: valid, 2 universities");
    if (!result.valid || result.byUniversity.size() != 2)
        return;

    const AdminUniversityShortlist &knownList = result.byUniversity[0];
    check(knownList.candidates.size() == 2, "flags: 2 candidates at the known-minimum university");
    for (int i = 0; i < knownList.candidates.size(); ++i)
    {
        const AdminCandidate &c = knownList.candidates[i];
        check(!c.minGpaUnknown, "flags: " + c.student.name + " has minGpaUnknown false (minimum is known)");
        if (c.student.name == "Normal")
            check(!c.statementMissing, "flags: Normal has statementMissing false");
        else if (c.student.name == "NoStatement")
            check(c.statementMissing, "flags: NoStatement has statementMissing true");
        else
            check(false, "flags: unexpected candidate at the known-minimum university");
    }

    const AdminUniversityShortlist &unknownList = result.byUniversity[1];
    check(unknownList.candidates.size() == 1, "flags: 1 candidate at the unknown-minimum university");
    if (unknownList.candidates.size() == 1)
    {
        check(unknownList.candidates[0].student.name == "AtUnknown", "flags: AtUnknown is the candidate there");
        check(unknownList.candidates[0].minGpaUnknown, "flags: AtUnknown has minGpaUnknown true");
        check(!unknownList.candidates[0].statementMissing, "flags: AtUnknown has statementMissing false");
    }
}

// ---------------------------------------------------------------------------
// r) Option 3: invalid and empty inputs
// ---------------------------------------------------------------------------

static void testShortlistInvalidAndEmpty()
{
    Vector<AdminStudent> students;
    Vector<AdminUniversity> universities;
    Vector<AdminSeatCount> counts;
    fillUsualExample(students, universities, counts);

    // All-zero weights.
    AdminAllocator zero(students, universities, counts, false, makeWeights(0.0, 0.0, 0.0, 0.0));
    AdminShortlistResult zeroResult = zero.runTopCandidates(0);
    check(!zeroResult.valid && zeroResult.byUniversity.size() == 0,
          "shortlist invalid weights: invalid and empty");

    // A negative admin seat count.
    Vector<AdminSeatCount> negativeCounts;
    negativeCounts.push_back(makeCount("uva", -1));
    AdminAllocator negative(students, universities, negativeCounts, false, defaultWeights());
    AdminShortlistResult negativeResult = negative.runTopCandidates(0);
    check(!negativeResult.valid && negativeResult.byUniversity.size() == 0,
          "shortlist negative seats: invalid and empty");

    // No students: valid, every university present, nobody listed.
    Vector<AdminStudent> noStudents;
    AdminAllocator empty(noStudents, universities, Vector<AdminSeatCount>(), false, defaultWeights());
    AdminShortlistResult emptyResult = empty.runTopCandidates(0);
    check(emptyResult.valid, "shortlist no students: valid");
    check(emptyResult.byUniversity.size() == 3, "shortlist no students: every university is present");
    for (int u = 0; u < emptyResult.byUniversity.size(); ++u)
    {
        check(emptyResult.byUniversity[u].candidates.size() == 0 && emptyResult.byUniversity[u].eligibleCount == 0,
              "shortlist no students: " + emptyResult.byUniversity[u].slug + " has no candidates");
    }

    // No universities: valid, nothing in the result.
    AdminAllocator lonely(students, Vector<AdminUniversity>(), Vector<AdminSeatCount>(), false, defaultWeights());
    AdminShortlistResult lonelyResult = lonely.runTopCandidates(0);
    check(lonelyResult.valid && lonelyResult.byUniversity.size() == 0,
          "shortlist no universities: valid and empty");
}

// ---------------------------------------------------------------------------
// s) Option 3: snapshot and independence
// ---------------------------------------------------------------------------

static void testShortlistSnapshotAndIndependence()
{
    // The allocator and its input lists are destroyed at the end of the block.
    AdminShortlistResult kept;
    {
        Vector<AdminStudent> students;
        Vector<AdminUniversity> universities;
        Vector<AdminSeatCount> counts;
        fillUsualExample(students, universities, counts);
        AdminAllocator allocator(students, universities, counts, false, defaultWeights());
        kept = allocator.runTopCandidates(0);
    }
    checkUsualShortlist(kept, "shortlist snapshot after lists are gone");

    // Changing the original lists after the allocator was built changes nothing.
    Vector<AdminStudent> students;
    Vector<AdminUniversity> universities;
    Vector<AdminSeatCount> counts;
    fillUsualExample(students, universities, counts);
    AdminAllocator allocator(students, universities, counts, false, defaultWeights());
    AdminShortlistResult before = allocator.runTopCandidates(0);

    students[0].name = "Changed";
    students[0].gpa = 1.0;
    students.push_back(makeStudentWithFirstChoice("Extra", 3.9, 9.9, "uva"));
    universities[0].slug = "renamed";
    counts[0].seats = 0;

    AdminShortlistResult after = allocator.runTopCandidates(0);
    check(sameShortlistResult(before, after), "shortlist snapshot: a later run ignores changes to the original lists");
    checkUsualShortlist(after, "shortlist snapshot: run after the lists changed");

    // Running option 3 around options 1 and 2 changes nothing for them.
    Vector<AdminStudent> students2;
    Vector<AdminUniversity> universities2;
    Vector<AdminSeatCount> counts2;
    fillUsualExample(students2, universities2, counts2);
    AdminAllocator plain(students2, universities2, counts2, false, defaultWeights());
    AdminMatchResult plainMost = plain.runMostPlaced();
    AdminMatchResult plainBest = plain.runBestFit();

    AdminAllocator mixed(students2, universities2, counts2, false, defaultWeights());
    AdminShortlistResult top1 = mixed.runTopCandidates(0);
    AdminMatchResult mixedMost = mixed.runMostPlaced();
    AdminMatchResult mixedBest = mixed.runBestFit();
    AdminShortlistResult top2 = mixed.runTopCandidates(0);
    AdminMatchResult mixedMostAgain = mixed.runMostPlaced();
    AdminMatchResult mixedBestAgain = mixed.runBestFit();

    check(sameResult(plainMost, mixedMost), "shortlist independence: most placed is the same after a shortlist");
    check(sameResult(plainBest, mixedBest), "shortlist independence: best fit is the same after a shortlist");
    check(sameResult(plainMost, mixedMostAgain), "shortlist independence: most placed is the same after two shortlists");
    check(sameResult(plainBest, mixedBestAgain), "shortlist independence: best fit is the same after two shortlists");
    check(sameShortlistResult(top1, top2), "shortlist independence: the shortlist is the same before and after the other runs");
    checkUsualShortlist(top2, "shortlist independence: second shortlist");

    // Calling it twice in a row gives the same answer.
    check(sameShortlistResult(mixed.runTopCandidates(2), mixed.runTopCandidates(2)),
          "shortlist independence: calling twice gives identical results");
}

// ---------------------------------------------------------------------------
// t, u) Option 3: random comparison against an independent reference
// ---------------------------------------------------------------------------

static void testShortlistRandomAgainstReference()
{
    Lcg rng(20240611ULL);

    for (int trial = 0; trial < 500; ++trial)
    {
        int failuresBefore = failures;
        std::string tag = "shortlist random trial " + std::to_string(trial);

        int studentCount = rng.nextInt(1, 8);
        int universityCount = rng.nextInt(1, 4);

        Vector<AdminUniversity> universities;
        for (int u = 0; u < universityCount; ++u)
        {
            AdminUniversity university = makeUniversity("u" + std::to_string(u));
            if (rng.nextInt(0, 2) == 0)  // about a third know their minimum GPA
            {
                university.minGpaKnown = true;
                university.minGpa = rng.nextDouble(2.8, 3.9);
            }
            universities.push_back(university);
        }

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
        int cap = rng.nextInt(0, 4);

        AdminAllocator allocator(students, universities, Vector<AdminSeatCount>(), allowUnranked, weights);
        AdminFitScorer scorer(students, universities, allowUnranked, weights);

        AdminShortlistResult result = allocator.runTopCandidates(cap);
        check(result.valid, tag + ": valid");
        check(result.byUniversity.size() == universityCount, tag + ": one entry per university");

        if (result.valid && result.byUniversity.size() == universityCount)
        {
            const Vector<PairResult> &pairs = scorer.pairs();
            for (int u = 0; u < universityCount; ++u)
            {
                // The reference list: the eligible pairs of this university, sorted by hand
                // (insertion sort) by score high to low, then by lower student position.
                Vector<int> refStudent;
                Vector<double> refScore;
                for (int i = 0; i < pairs.size(); ++i)
                {
                    if (pairs[i].universityIndex != u || !pairs[i].eligible())
                        continue;

                    int newStudent = pairs[i].studentIndex;
                    double newScore = pairs[i].score;
                    refStudent.push_back(newStudent);
                    refScore.push_back(newScore);
                    int pos = refStudent.size() - 1;
                    while (pos > 0 && (refScore[pos - 1] < newScore ||
                                       (refScore[pos - 1] == newScore && refStudent[pos - 1] > newStudent)))
                    {
                        refStudent[pos] = refStudent[pos - 1];
                        refScore[pos] = refScore[pos - 1];
                        --pos;
                    }
                    refStudent[pos] = newStudent;
                    refScore[pos] = newScore;
                }

                const AdminUniversityShortlist &shortlist = result.byUniversity[u];
                int refLength = refStudent.size();
                int expectedListed = (cap > 0 && cap < refLength) ? cap : refLength;

                check(shortlist.slug == universities[u].slug, tag + ": university order is kept");
                check(shortlist.eligibleCount == refLength, tag + ": eligibleCount matches the reference");
                check(shortlist.candidates.size() == expectedListed, tag + ": number listed matches min(cap, eligible)");
                check(listIsOrdered(shortlist), tag + ": ranks are 1, 2, 3, ... and scores never increase");

                if (shortlist.candidates.size() == expectedListed)
                {
                    for (int i = 0; i < expectedListed; ++i)
                    {
                        const AdminCandidate &c = shortlist.candidates[i];
                        check(c.rank == i + 1, tag + ": rank matches the reference");
                        check(c.student.name == students[refStudent[i]].name, tag + ": student matches the reference");
                        check(near(c.score, refScore[i], 1e-12), tag + ": score matches the reference");
                    }
                }
            }
        }

        if (failures != failuresBefore)
        {
            std::cout << "FAIL: shortlist random trial " << trial << " of 500 had failing checks (see above); stopping\n";
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Option 4 (runHighestScore): the six new cases
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// option 4, case 1: the usual example
// ---------------------------------------------------------------------------

static void testHighestScoreUsualExample()
{
    Vector<AdminStudent> students;
    Vector<AdminUniversity> universities;
    Vector<AdminSeatCount> counts;
    fillUsualExample(students, universities, counts);

    AdminAllocator allocator(students, universities, counts, false, defaultWeights());
    check(allocator.valid(), "option 4, case 1: allocator is valid");
    checkUsualResult(allocator.runHighestScore(), "option 4, case 1: usual example");
}

// ---------------------------------------------------------------------------
// option 4, case 2: the case where the options differ
// ---------------------------------------------------------------------------

static void testHighestScoreDiffersFromBestFit()
{
    // A only has X (third choice, score about 0.4). B has X (first choice, score about 1.0)
    // and Y (third choice). Option 4 puts the highest total score first: B at X alone scores
    // about 1.0, which beats A at X plus B at Y (about 0.8). Option 2 puts the most students
    // placed first: it places both (A at X, B at Y) even though the total score is lower.
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
    check(allocator.valid(), "option 4, case 2: allocator is valid");

    AdminMatchResult highest = allocator.runHighestScore();
    check(highest.valid, "option 4, case 2: highest score is valid");
    int x = findUniversity(highest, "X");
    int y = findUniversity(highest, "Y");
    check(x >= 0 && y >= 0, "option 4, case 2: both universities are in the result");
    if (x >= 0 && y >= 0)
    {
        check(highest.byUniversity[x].placed.size() == 1 && findPlaced(highest.byUniversity[x], "B") == 0,
              "option 4, case 2: only B is placed, at X");
        check(highest.byUniversity[x].placed.size() == 1 &&
                  near(highest.byUniversity[x].placed[0].score, 1.0, 0.005),
              "option 4, case 2: B's score at X is about 1.0");
        check(highest.byUniversity[y].placed.size() == 0, "option 4, case 2: nobody is placed at Y");
    }
    check(highest.unplaced.size() == 1 && isUnplaced(highest, "A"), "option 4, case 2: A is the unplaced student");

    // On the same allocator, options 2 and 1 place both students.
    checkBothPlaced(allocator.runBestFit(), "option 4, case 2: best fit");
    checkBothPlaced(allocator.runMostPlaced(), "option 4, case 2: most placed");
}

// ---------------------------------------------------------------------------
// option 4, case 3: capacity
// ---------------------------------------------------------------------------

static void testHighestScoreCapacity()
{
    Vector<AdminStudent> students;
    for (int i = 0; i < 5; ++i)
        students.push_back(makeStudentWithFirstChoice("S" + std::to_string(i), 3.0 + 0.1 * i, 7.0 + 0.2 * i, "uva"));

    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 3));

    AdminAllocator allocator(students, universities, counts, false, defaultWeights());
    AdminMatchResult result = allocator.runHighestScore();

    check(result.valid, "option 4, case 3: valid");
    check(countPlaced(result) <= 3, "option 4, case 3: at most 3 placed");
    check(result.byUniversity.size() == 1 && result.byUniversity[0].totalSeats == 3 &&
              result.byUniversity[0].placed.size() <= result.byUniversity[0].totalSeats,
          "option 4, case 3: uva is never over its 3 seats");
    check(countPlaced(result) + result.unplaced.size() == 5, "option 4, case 3: placed + unplaced = 5");
}

// ---------------------------------------------------------------------------
// option 4, case 4: invalid and empty inputs
// ---------------------------------------------------------------------------

static void testHighestScoreInvalidAndEmpty()
{
    Vector<AdminStudent> students;
    Vector<AdminUniversity> universities;
    Vector<AdminSeatCount> counts;
    fillUsualExample(students, universities, counts);

    // All-zero weights.
    AdminAllocator zero(students, universities, counts, false, makeWeights(0.0, 0.0, 0.0, 0.0));
    check(!zero.valid(), "option 4, case 4: all-zero weights, valid() is false");
    AdminMatchResult zeroResult = zero.runHighestScore();
    check(!zeroResult.valid && zeroResult.byUniversity.size() == 0 && zeroResult.unplaced.size() == 0,
          "option 4, case 4: all-zero weights, result is invalid and empty");

    // A later valid setWeights fixes it.
    check(zero.setWeights(defaultWeights()), "option 4, case 4: setWeights with valid weights returns true");
    check(zero.valid(), "option 4, case 4: valid() is true after valid weights");
    check(zero.runHighestScore().valid, "option 4, case 4: the run is valid after valid weights");

    // A negative admin seat count.
    Vector<AdminSeatCount> negativeCounts;
    negativeCounts.push_back(makeCount("uva", -1));
    AdminAllocator negative(students, universities, negativeCounts, false, defaultWeights());
    AdminMatchResult negativeResult = negative.runHighestScore();
    check(!negative.valid(), "option 4, case 4: negative seats, valid() is false");
    check(!negativeResult.valid && negativeResult.byUniversity.size() == 0,
          "option 4, case 4: negative seats, result is invalid and empty");
    negative.setWeights(defaultWeights());
    check(!negative.valid() && !negative.runHighestScore().valid,
          "option 4, case 4: negative seats, still invalid after valid weights");

    // No students.
    Vector<AdminStudent> noStudents;
    Vector<AdminUniversity> threeUniversities;
    threeUniversities.push_back(makeUniversity("uva"));
    threeUniversities.push_back(makeUniversity("tum"));
    threeUniversities.push_back(makeUniversity("sciences-po"));
    AdminAllocator empty(noStudents, threeUniversities, Vector<AdminSeatCount>(), false, defaultWeights());
    checkNoStudents(empty.runHighestScore(), "option 4, case 4: no students");

    // No universities.
    Vector<AdminStudent> twoStudents;
    twoStudents.push_back(makeStudentWithFirstChoice("One", 3.5, 8.0, "uva"));
    twoStudents.push_back(makeStudentWithFirstChoice("Two", 3.6, 8.5, "tum"));
    AdminAllocator lonely(twoStudents, Vector<AdminUniversity>(), Vector<AdminSeatCount>(), false, defaultWeights());
    checkNoUniversities(lonely.runHighestScore(), "option 4, case 4: no universities");
}

// ---------------------------------------------------------------------------
// option 4, case 5: snapshot and independence
// ---------------------------------------------------------------------------

static void testHighestScoreSnapshotAndIndependence()
{
    // The allocator and its input lists are destroyed at the end of the block.
    AdminMatchResult kept;
    {
        Vector<AdminStudent> students;
        Vector<AdminUniversity> universities;
        Vector<AdminSeatCount> counts;
        fillUsualExample(students, universities, counts);
        AdminAllocator allocator(students, universities, counts, false, defaultWeights());
        kept = allocator.runHighestScore();
    }
    checkUsualResult(kept, "option 4, case 5: snapshot after lists are gone");

    // Changing the original lists after the allocator was built changes nothing.
    Vector<AdminStudent> students;
    Vector<AdminUniversity> universities;
    Vector<AdminSeatCount> counts;
    fillUsualExample(students, universities, counts);
    AdminAllocator allocator(students, universities, counts, false, defaultWeights());
    AdminMatchResult before = allocator.runHighestScore();

    students[0].name = "Changed";
    students[0].gpa = 1.0;
    students.push_back(makeStudentWithFirstChoice("Extra", 3.9, 9.9, "uva"));
    universities[0].slug = "renamed";
    counts[0].seats = 0;

    AdminMatchResult after = allocator.runHighestScore();
    check(sameResult(before, after), "option 4, case 5: a later run ignores changes to the original lists");
    checkUsualResult(after, "option 4, case 5: run after the lists changed");

    // Running option 4 between options 1 and 2 changes nothing for them.
    Vector<AdminStudent> students2;
    Vector<AdminUniversity> universities2;
    Vector<AdminSeatCount> counts2;
    fillUsualExample(students2, universities2, counts2);
    AdminAllocator plain(students2, universities2, counts2, false, defaultWeights());
    AdminMatchResult plainMost = plain.runMostPlaced();
    AdminMatchResult plainBest = plain.runBestFit();

    AdminAllocator mixed(students2, universities2, counts2, false, defaultWeights());
    AdminMatchResult mixedMost = mixed.runMostPlaced();
    AdminMatchResult highest1 = mixed.runHighestScore();
    AdminMatchResult mixedBest = mixed.runBestFit();
    AdminMatchResult highest2 = mixed.runHighestScore();

    check(sameResult(plainMost, mixedMost), "option 4, case 5: most placed is unchanged by option 4");
    check(sameResult(plainBest, mixedBest), "option 4, case 5: best fit is unchanged by option 4");
    check(sameResult(plainMost, mixed.runMostPlaced()), "option 4, case 5: most placed is unchanged afterwards");
    check(sameResult(plainBest, mixed.runBestFit()), "option 4, case 5: best fit is unchanged afterwards");
    check(sameResult(highest1, highest2), "option 4, case 5: running option 4 twice gives identical results");
}

// ---------------------------------------------------------------------------
// option 4, case 6: random comparison against brute force
// ---------------------------------------------------------------------------

// Like BruteForce, but it remembers the best TOTAL SCORE over all ways (whatever the
// number placed) and, separately, the best NUMBER placed.
struct BestTotalBruteForce
{
    const Vector<AdminSeatEdge> &edges;
    int studentCount;
    Vector<bool> used;   // which seats are taken right now
    int bestCount;
    double bestTotal;

    BestTotalBruteForce(const Vector<AdminSeatEdge> &edgeList, int students, int seats)
        : edges(edgeList), studentCount(students), used(seats, false), bestCount(0), bestTotal(0.0)
    {
    }

    void go(int student, int count, double total)
    {
        if (student == studentCount)
        {
            if (count > bestCount)
                bestCount = count;
            if (total > bestTotal)
                bestTotal = total;
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

static void testHighestScoreRandomAgainstBruteForce()
{
    Lcg rng(20240612ULL);

    for (int trial = 0; trial < 500; ++trial)
    {
        int failuresBefore = failures;
        std::string tag = "option 4, case 6, random trial " + std::to_string(trial);

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

        BestTotalBruteForce brute(edges, studentCount, table.seatCount());
        brute.go(0, 0, 0.0);

        AdminMatchResult highest = allocator.runHighestScore();
        AdminMatchResult best = allocator.runBestFit();

        checkRandomRun(highest, table, studentCount, tag);
        check(near(totalScore(highest), brute.bestTotal, 1e-9), tag + ": total score equals the best possible total");
        check(countPlaced(highest) <= brute.bestCount, tag + ": places no more than the best possible number");
        check(totalScore(highest) >= totalScore(best) - 1e-9, tag + ": total is at least best fit's total");
        check(countPlaced(highest) <= countPlaced(best), tag + ": places no more students than best fit");

        if (failures != failuresBefore)
        {
            std::cout << "FAIL: option 4, case 6, random trial " << trial << " of 500 had failing checks (see above); stopping\n";
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
    testShortlistUsualExample();
    testShortlistCap();
    testShortlistAllowUnranked();
    testShortlistTies();
    testShortlistFlags();
    testShortlistInvalidAndEmpty();
    testShortlistSnapshotAndIndependence();
    testShortlistRandomAgainstReference();
    testHighestScoreUsualExample();
    testHighestScoreDiffersFromBestFit();
    testHighestScoreCapacity();
    testHighestScoreInvalidAndEmpty();
    testHighestScoreSnapshotAndIndependence();
    testHighestScoreRandomAgainstBruteForce();

    if (failures == 0)
        std::cout << "All admin allocator checks passed\n";
    else
        std::cout << failures << " check(s) failed\n";
    return failures == 0 ? 0 : 1;
}
