// Self-contained test for AdminSeatTable. No test framework.
// main() returns 0 when every check passes, and 1 when any check fails.
// Each failed check prints its name so you can see which one broke.

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "vector.hpp"
#include "services/admin_scoring.hpp"
#include "services/admin_seats.hpp"

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

// The default weights from the worked example.
static ScoringWeights defaultWeights()
{
    ScoringWeights w;
    w.preference = 0.65;
    w.gpa = 0.45;
    w.statement = 0.35;
    w.fairness = 0.20;
    return w;
}

// A university with no restrictions, optionally accepting "ALL" majors.
static AdminUniversity makeUniversity(const std::string &slug)
{
    AdminUniversity u;
    u.slug = slug;
    u.name = slug;
    u.acceptedMajors.push_back("ALL");
    return u;
}

// A student who passes every check for "uva" as their first choice.
static AdminStudent makeStudent(const std::string &name, double gpa, double statement, int uvaRank)
{
    AdminStudent s;
    s.id = 1;
    s.name = name;
    s.gpa = gpa;
    s.majorCode = "CSCE";
    s.semester = "Fall";
    s.prefs[uvaRank] = "uva";
    s.statementRating = statement;
    s.hasStatementRating = true;
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

// True when asking the table for this seat throws std::out_of_range.
static bool slotThrows(const AdminSeatTable &table, int index)
{
    try
    {
        table.slot(index);
    }
    catch (const std::out_of_range &)
    {
        return true;
    }
    return false;
}

// True when asking the table for this university throws std::out_of_range.
static bool universitySeatsThrows(const AdminSeatTable &table, int index)
{
    try
    {
        table.universitySeats(index);
    }
    catch (const std::out_of_range &)
    {
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// a) The worked example with 2 seats at uva
// ---------------------------------------------------------------------------

static void testWorkedExample()
{
    Vector<AdminStudent> students;
    students.push_back(makeStudent("Youssef", 3.78, 8.9, 0));
    students.push_back(makeStudent("Laila", 3.85, 9.0, 1));
    students.push_back(makeStudent("Salma", 3.92, 9.2, 2));

    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));

    AdminFitScorer scorer(students, universities, false, defaultWeights());

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 2));
    AdminSeatTable table(universities, counts);

    check(table.valid(), "worked example: table is valid");
    check(table.universityCount() == 1, "worked example: 1 university");
    check(table.seatCount() == 2, "worked example: 2 seats");
    check(table.slot(0).label == "uva #1", "worked example: first seat is 'uva #1'");
    check(table.slot(1).label == "uva #2", "worked example: second seat is 'uva #2'");
    check(table.slot(0).seatNumber == 1 && table.slot(1).seatNumber == 2,
          "worked example: seat numbers 1 and 2");

    Vector<AdminSeatEdge> edges = table.buildEdges(scorer.pairs());
    check(edges.size() == 6, "worked example: 6 edges");
    if (edges.size() == 6)
    {
        const double expected[3] = {0.93, 0.83, 0.73};
        for (int s = 0; s < 3; ++s)
        {
            int count = 0;
            for (int i = 0; i < edges.size(); ++i)
            {
                if (edges[i].studentIndex != s)
                    continue;
                ++count;
                check(near(edges[i].score, expected[s], 0.005),
                      "worked example: edge score matches student " + std::to_string(s));
                check(edges[i].seatIndex == 0 || edges[i].seatIndex == 1,
                      "worked example: edge points at a uva seat");
            }
            check(count == 2, "worked example: student " + std::to_string(s) + " has 2 edges");
        }
    }
}

// ---------------------------------------------------------------------------
// b) 10 seats, nobody eligible
// ---------------------------------------------------------------------------

static void testSeatsWithoutApplicants()
{
    Vector<AdminStudent> students;
    AdminStudent s = makeStudent("Blocked", 3.5, 8.0, 0);
    s.status = "Ineligible";
    students.push_back(s);

    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));
    AdminFitScorer scorer(students, universities, false, defaultWeights());

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 10));
    AdminSeatTable table(universities, counts);

    check(table.valid(), "no applicants: table is valid");
    check(table.seatCount() == 10, "no applicants: all 10 seats exist");
    check(table.universitySeats(0).totalSeats == 10, "no applicants: totalSeats is 10");
    check(table.buildEdges(scorer.pairs()).size() == 0, "no applicants: 0 edges");

    // Also with no students at all.
    Vector<AdminStudent> none;
    AdminFitScorer emptyScorer(none, universities, false, defaultWeights());
    check(table.buildEdges(emptyScorer.pairs()).size() == 0, "no students at all: 0 edges");
}

// ---------------------------------------------------------------------------
// c) 0 seats
// ---------------------------------------------------------------------------

static void testZeroSeats()
{
    Vector<AdminStudent> students;
    students.push_back(makeStudent("Youssef", 3.78, 8.9, 0));
    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));
    AdminFitScorer scorer(students, universities, false, defaultWeights());
    check(scorer.pair(0).eligible(), "zero seats: the pair itself is eligible");

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 0));
    AdminSeatTable table(universities, counts);

    check(table.valid(), "zero seats: table is valid");
    check(table.seatCount() == 0, "zero seats: no seats");
    check(table.universitySeats(0).totalSeats == 0, "zero seats: totalSeats is 0");
    check(table.universitySeats(0).firstSeatIndex == -1, "zero seats: firstSeatIndex is -1");
    check(!table.universitySeats(0).assumed, "zero seats: set by admin, not assumed");
    check(table.buildEdges(scorer.pairs()).size() == 0, "zero seats: no edges");
    check(slotThrows(table, 0), "zero seats: slot(0) throws");
}

// ---------------------------------------------------------------------------
// d) assumed vs set by admin
// ---------------------------------------------------------------------------

static void testAssumed()
{
    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));
    universities.push_back(makeUniversity("mit"));

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 3));
    AdminSeatTable table(universities, counts);

    check(table.valid(), "assumed: table is valid");
    check(table.universitySeats(0).totalSeats == 3, "assumed: admin-set university has 3 seats");
    check(!table.universitySeats(0).assumed, "assumed: admin-set university is not assumed");
    check(table.universitySeats(1).totalSeats == DEFAULT_SEATS_PER_UNIVERSITY,
          "assumed: unset university gets the default seats");
    check(table.universitySeats(1).assumed, "assumed: unset university is assumed");
    check(table.seatCount() == 3 + DEFAULT_SEATS_PER_UNIVERSITY, "assumed: total seat count");
    check(table.universitySeats(0).firstSeatIndex == 0, "assumed: first university starts at seat 0");
    check(table.universitySeats(1).firstSeatIndex == 3, "assumed: second university starts at seat 3");

    // A custom default is used too.
    AdminSeatTable custom(universities, Vector<AdminSeatCount>(), 5);
    check(custom.seatCount() == 10, "assumed: custom default of 5 gives 10 seats");
}

// ---------------------------------------------------------------------------
// e) ineligible pairs produce no edges
// ---------------------------------------------------------------------------

static void testIneligiblePairs()
{
    Vector<AdminStudent> students;
    AdminStudent ineligible = makeStudent("Ineligible", 3.5, 8.0, 0);
    ineligible.status = "Ineligible";
    students.push_back(ineligible);
    students.push_back(makeStudent("LowGpa", 2.5, 8.0, 0));
    students.push_back(makeStudent("Good", 3.5, 8.0, 0));

    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));
    AdminFitScorer scorer(students, universities, false, defaultWeights());

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 2));
    AdminSeatTable table(universities, counts);

    Vector<AdminSeatEdge> edges = table.buildEdges(scorer.pairs());
    check(edges.size() == 2, "ineligible: only the eligible student gets edges");
    for (int i = 0; i < edges.size(); ++i)
        check(edges[i].studentIndex == 2, "ineligible: every edge belongs to the eligible student");
}

// ---------------------------------------------------------------------------
// f) universityOfSeat and bad indexes
// ---------------------------------------------------------------------------

static void testLookups()
{
    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("a"));
    universities.push_back(makeUniversity("b"));
    universities.push_back(makeUniversity("c"));

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("a", 2));
    counts.push_back(makeCount("b", 0));
    counts.push_back(makeCount("c", 3));
    AdminSeatTable table(universities, counts);

    // Seats: a0 a1 c2 c3 c4
    check(table.seatCount() == 5, "lookups: 5 seats");
    check(table.universityOfSeat(0) == 0, "lookups: seat 0 is at university 0");
    check(table.universityOfSeat(1) == 0, "lookups: seat 1 is at university 0");
    check(table.universityOfSeat(2) == 2, "lookups: seat 2 is at university 2");
    check(table.universityOfSeat(4) == 2, "lookups: seat 4 is at university 2");
    check(table.universityOfSeat(-1) == -1, "lookups: negative seat gives -1");
    check(table.universityOfSeat(5) == -1, "lookups: seat just past the end gives -1");
    check(table.universityOfSeat(1000) == -1, "lookups: huge seat gives -1");

    check(table.slot(2).label == "c #1", "lookups: seat 2 is 'c #1'");
    check(table.slot(4).label == "c #3", "lookups: seat 4 is 'c #3'");
    check(table.slot(4).universityIndex == 2, "lookups: slot knows its university");

    check(slotThrows(table, -1), "lookups: slot(-1) throws");
    check(slotThrows(table, 5), "lookups: slot(too large) throws");
    check(!slotThrows(table, 4), "lookups: slot(last) does not throw");
    check(universitySeatsThrows(table, -1), "lookups: universitySeats(-1) throws");
    check(universitySeatsThrows(table, 3), "lookups: universitySeats(too large) throws");
    check(!universitySeatsThrows(table, 2), "lookups: universitySeats(last) does not throw");
}

// ---------------------------------------------------------------------------
// g) countSeatsLeft
// ---------------------------------------------------------------------------

static void testCountSeatsLeft()
{
    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));
    universities.push_back(makeUniversity("b"));
    universities.push_back(makeUniversity("c"));

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 10));
    counts.push_back(makeCount("c", 0));
    AdminSeatTable table(universities, counts);  // b gets the default

    Vector<int> placed;
    placed.push_back(0);
    placed.push_back(5);
    AdminSeatUsage usage = table.countSeatsLeft(placed);

    check(usage.valid, "usage: valid");
    check(usage.usage.size() == 3, "usage: one entry per university");
    if (usage.usage.size() == 3)
    {
        check(usage.usage[0].universityIndex == 0, "usage: uva index");
        check(usage.usage[0].totalSeats == 10, "usage: uva total 10");
        check(usage.usage[0].filled == 2, "usage: uva filled 2");
        check(usage.usage[0].left == 8, "usage: uva left 8");
        check(usage.usage[1].filled == 0, "usage: b filled 0");
        check(usage.usage[1].left == DEFAULT_SEATS_PER_UNIVERSITY, "usage: b left is its total");
        check(usage.usage[2].totalSeats == 0 && usage.usage[2].filled == 0 && usage.usage[2].left == 0,
              "usage: 0-seat university shows 0 and 0");
    }

    // Nobody placed at all.
    AdminSeatUsage empty = table.countSeatsLeft(Vector<int>());
    check(empty.valid, "usage: empty placed list is valid");
    check(empty.usage.size() == 3 && empty.usage[0].left == 10, "usage: empty placed list leaves all seats");

    // Seats in a later university are credited to that university.
    Vector<int> second;
    second.push_back(10);  // first seat of b
    AdminSeatUsage later = table.countSeatsLeft(second);
    check(later.valid && later.usage.size() == 3, "usage: later university placement is valid");
    if (later.valid && later.usage.size() == 3)
    {
        check(later.usage[1].filled == 1, "usage: b filled 1");
        check(later.usage[0].filled == 0, "usage: uva filled 0 when only b used");
    }
}

// ---------------------------------------------------------------------------
// h) invalid cases
// ---------------------------------------------------------------------------

static void testInvalid()
{
    Vector<AdminStudent> students;
    students.push_back(makeStudent("Youssef", 3.78, 8.9, 0));
    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));
    AdminFitScorer scorer(students, universities, false, defaultWeights());

    // Negative admin count for a listed university.
    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", -1));
    AdminSeatTable negative(universities, counts);
    check(!negative.valid(), "invalid: negative admin count makes table invalid");
    check(negative.seatCount() == 0, "invalid: negative admin count leaves 0 seats");
    check(negative.universityCount() == 0, "invalid: negative admin count leaves universityCount 0");
    check(negative.buildEdges(scorer.pairs()).size() == 0, "invalid: negative admin count gives 0 edges");
    check(!negative.countSeatsLeft(Vector<int>()).valid, "invalid: countSeatsLeft on invalid table is invalid");
    check(slotThrows(negative, 0), "invalid: slot(0) throws on invalid table");
    check(universitySeatsThrows(negative, 0), "invalid: universitySeats(0) throws on invalid table");

    // Negative default.
    AdminSeatTable negativeDefault(universities, Vector<AdminSeatCount>(), -3);
    check(!negativeDefault.valid(), "invalid: negative default makes table invalid");
    check(negativeDefault.seatCount() == 0, "invalid: negative default leaves 0 seats");
    check(negativeDefault.universityCount() == 0, "invalid: negative default leaves universityCount 0");
    check(negativeDefault.buildEdges(scorer.pairs()).size() == 0, "invalid: negative default gives 0 edges");
    check(!negativeDefault.countSeatsLeft(Vector<int>()).valid,
          "invalid: countSeatsLeft on negative-default table is invalid");

    // A negative default is harmless when the admin set every listed university.
    Vector<AdminSeatCount> allSet;
    allSet.push_back(makeCount("uva", 2));
    AdminSeatTable covered(universities, allSet, -3);
    check(!covered.valid(), "invalid: negative default is refused even if admin set everything");

    // Bad placed lists on a valid table.
    Vector<AdminSeatCount> good;
    good.push_back(makeCount("uva", 3));
    AdminSeatTable table(universities, good);

    Vector<int> twice;
    twice.push_back(1);
    twice.push_back(1);
    check(!table.countSeatsLeft(twice).valid, "invalid: same seat twice");

    Vector<int> tooBig;
    tooBig.push_back(0);
    tooBig.push_back(3);
    check(!table.countSeatsLeft(tooBig).valid, "invalid: seat index too large");

    Vector<int> negativeSeat;
    negativeSeat.push_back(-1);
    check(!table.countSeatsLeft(negativeSeat).valid, "invalid: negative seat index");

    Vector<int> fine;
    fine.push_back(0);
    fine.push_back(2);
    check(table.countSeatsLeft(fine).valid, "invalid: a clean placed list is still valid");
}

// ---------------------------------------------------------------------------
// i) duplicate admin slugs and unknown slugs
// ---------------------------------------------------------------------------

static void testAdminEntries()
{
    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));

    Vector<AdminSeatCount> dup;
    dup.push_back(makeCount("uva", 5));
    dup.push_back(makeCount("uva", 3));
    AdminSeatTable table(universities, dup);
    check(table.valid(), "duplicates: table is valid");
    check(table.seatCount() == 3, "duplicates: the later entry wins");

    // A bad earlier entry is replaced by a good later one.
    Vector<AdminSeatCount> fixed;
    fixed.push_back(makeCount("uva", -1));
    fixed.push_back(makeCount("uva", 4));
    AdminSeatTable replaced(universities, fixed);
    check(replaced.valid() && replaced.seatCount() == 4,
          "duplicates: a later good entry replaces an earlier negative one");

    // And a bad later entry makes it invalid.
    Vector<AdminSeatCount> broken;
    broken.push_back(makeCount("uva", 4));
    broken.push_back(makeCount("uva", -1));
    check(!AdminSeatTable(universities, broken).valid(),
          "duplicates: a later negative entry makes the table invalid");

    // Unknown slugs are ignored.
    Vector<AdminSeatCount> unknown;
    unknown.push_back(makeCount("nowhere", 50));
    unknown.push_back(makeCount("uva", 2));
    AdminSeatTable ignored(universities, unknown);
    check(ignored.valid(), "unknown slug: table is valid");
    check(ignored.universityCount() == 1, "unknown slug: still 1 university");
    check(ignored.seatCount() == 2, "unknown slug: its seats are not added");
}

// ---------------------------------------------------------------------------
// j) 155 universities with the default seat count
// ---------------------------------------------------------------------------

static void testManyUniversities()
{
    Vector<AdminUniversity> universities;
    for (int i = 0; i < 155; ++i)
        universities.push_back(makeUniversity("u" + std::to_string(i)));

    AdminSeatTable table(universities, Vector<AdminSeatCount>());
    check(table.valid(), "155 universities: valid");
    check(table.universityCount() == 155, "155 universities: universityCount");
    check(table.seatCount() == 155 * DEFAULT_SEATS_PER_UNIVERSITY, "155 universities: exact seat count");
    if (table.seatCount() == 155 * DEFAULT_SEATS_PER_UNIVERSITY)
    {
        const AdminSeatSlot &last = table.slot(table.seatCount() - 1);
        check(last.universityIndex == 154, "155 universities: last seat belongs to the last university");
        check(last.label == "u154 #" + std::to_string(DEFAULT_SEATS_PER_UNIVERSITY),
              "155 universities: last seat label");
        check(table.universityOfSeat(table.seatCount() - 1) == 154, "155 universities: way back works");
    }
}

// ---------------------------------------------------------------------------
// k) negative count for an unknown slug does not invalidate the table
// ---------------------------------------------------------------------------

static void testNegativeForUnknownSlug()
{
    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));

    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("ghost", -5));
    counts.push_back(makeCount("uva", 2));
    AdminSeatTable table(universities, counts);

    check(table.valid(), "negative unknown slug: table stays valid");
    check(table.seatCount() == 2, "negative unknown slug: uva keeps its 2 seats");
}

// ---------------------------------------------------------------------------
// l) buildEdges skips a pair with a bad universityIndex
// ---------------------------------------------------------------------------

static void testBadUniversityIndex()
{
    Vector<AdminUniversity> universities;
    universities.push_back(makeUniversity("uva"));
    Vector<AdminSeatCount> counts;
    counts.push_back(makeCount("uva", 2));
    AdminSeatTable table(universities, counts);

    Vector<PairResult> pairs;

    PairResult tooBig;
    tooBig.studentIndex = 0;
    tooBig.universityIndex = 99;
    tooBig.score = 0.5;
    pairs.push_back(tooBig);

    PairResult negative;
    negative.studentIndex = 1;
    negative.universityIndex = -1;
    negative.score = 0.5;
    pairs.push_back(negative);

    PairResult good;
    good.studentIndex = 2;
    good.universityIndex = 0;
    good.score = 0.7;
    pairs.push_back(good);

    Vector<AdminSeatEdge> edges = table.buildEdges(pairs);
    check(edges.size() == 2, "bad university index: only the good pair makes edges");
    for (int i = 0; i < edges.size(); ++i)
    {
        check(edges[i].studentIndex == 2, "bad university index: edge belongs to the good pair");
        check(near(edges[i].score, 0.7, 0.0001), "bad university index: score copied unchanged");
    }
}

// ---------------------------------------------------------------------------
// m) the table does not depend on the lists that built it
// ---------------------------------------------------------------------------

static void testOutlivesInputs()
{
    // Start with an empty table, then replace it with one built from lists
    // that live only inside the block.
    AdminSeatTable table((Vector<AdminUniversity>()), (Vector<AdminSeatCount>()));
    {
        Vector<AdminUniversity> universities;
        universities.push_back(makeUniversity("uva"));
        universities.push_back(makeUniversity("mit"));
        Vector<AdminSeatCount> counts;
        counts.push_back(makeCount("uva", 3));
        table = AdminSeatTable(universities, counts);
        // universities and counts are destroyed here
    }

    check(table.valid(), "outlives inputs: still valid");
    check(table.seatCount() == 3 + DEFAULT_SEATS_PER_UNIVERSITY, "outlives inputs: seat count");
    check(table.slot(0).label == "uva #1", "outlives inputs: labels still readable");
    check(table.universityOfSeat(3) == 1, "outlives inputs: way back still works");

    PairResult pair;
    pair.studentIndex = 0;
    pair.universityIndex = 0;
    pair.score = 0.6;
    Vector<PairResult> pairs;
    pairs.push_back(pair);
    check(table.buildEdges(pairs).size() == 3, "outlives inputs: edges still built");

    Vector<int> placed;
    placed.push_back(0);
    check(table.countSeatsLeft(placed).valid, "outlives inputs: countSeatsLeft still works");
}

int main()
{
    testWorkedExample();
    testSeatsWithoutApplicants();
    testZeroSeats();
    testAssumed();
    testIneligiblePairs();
    testLookups();
    testCountSeatsLeft();
    testInvalid();
    testAdminEntries();
    testManyUniversities();
    testNegativeForUnknownSlug();
    testBadUniversityIndex();
    testOutlivesInputs();

    if (failures == 0)
        std::cout << "All admin seat checks passed\n";
    else
        std::cout << failures << " check(s) failed\n";
    return failures == 0 ? 0 : 1;
}
