#include "services/admin_allocator.hpp"

#include "bipartite.hpp"
#include "heap.hpp"

namespace
{

// Adds every seat edge to the graph. The bonus is added to each score: 0 for option 1
// (it ignores scores anyway), a large number for option 2 (see runBestFit).
void addEdgesToGraph(const Vector<AdminSeatEdge> &edges, double bonus,
                     BipartiteGraph<int, int> &graph)
{
    for (int i = 0; i < edges.size(); ++i)
        graph.add_edge(edges[i].studentIndex, edges[i].seatIndex, edges[i].score + bonus);
}

// One flat list holding the score of every (student, university) pair, so a placed
// student's score can be found instantly. The entry for student s at university u is
// at position s * universityCount + u. Pairs that are not eligible stay 0.
Vector<double> buildScoreLookup(const Vector<PairResult> &pairs, int studentCount, int universityCount)
{
    Vector<double> scoreOf(studentCount * universityCount, 0.0);
    for (int i = 0; i < pairs.size(); ++i)
    {
        const PairResult &pair = pairs[i];
        if (pair.eligible())
            scoreOf[pair.studentIndex * universityCount + pair.universityIndex] = pair.score;
    }
    return scoreOf;
}

// For every (student, university) pair that is eligible, the position of that pair in the
// scorer's list. Pairs that are not eligible get -1. The entry for student s at university u
// is at position s * universityCount + u. Used to read a candidate's warning flags.
Vector<int> buildPairLookup(const Vector<PairResult> &pairs, int studentCount, int universityCount)
{
    Vector<int> where(studentCount * universityCount, -1);
    for (int i = 0; i < pairs.size(); ++i)
    {
        const PairResult &pair = pairs[i];
        if (pair.eligible())
            where[pair.studentIndex * universityCount + pair.universityIndex] = i;
    }
    return where;
}

// Turns the matcher's answer (pairs of student number and seat number) into the
// snapshot result: who is placed at each university, and who is not placed.
// Returns an invalid (empty) result if the answer is impossible (a bug upstream).
AdminMatchResult buildResult(const Vector<AdminStudent> &students,
                             const Vector<AdminUniversity> &universities,
                             const AdminSeatTable &seats,
                             const AdminFitScorer &scorer,
                             const Vector<Pair<int, int>> &matched)
{
    AdminMatchResult result; // starts invalid and empty
    int universityCount = universities.size();

    // One section per university, even if nobody ends up there.
    for (int u = 0; u < universityCount; ++u)
    {
        AdminUniversityResult section;
        section.slug = universities[u].slug;
        section.name = universities[u].name;
        section.totalSeats = seats.universitySeats(u).totalSeats;
        section.seatsAssumed = seats.universitySeats(u).assumed;
        result.byUniversity.push_back(section);
    }

    Vector<double> scoreOf = buildScoreLookup(scorer.pairs(), students.size(), universityCount);
    Vector<bool> isPlaced(students.size(), false);
    Vector<int> placedSeats;

    for (int i = 0; i < matched.size(); ++i)
    {
        int student = matched[i].first;
        int seat = matched[i].second;
        int university = seats.universityOfSeat(seat);
        if (student < 0 || student >= students.size() || university < 0)
            return AdminMatchResult(); // impossible answer: report invalid

        AdminPlacedStudent placed;
        placed.student = students[student];
        placed.seatLabel = seats.slot(seat).label;
        placed.score = scoreOf[student * universityCount + university];
        result.byUniversity[university].placed.push_back(placed);

        isPlaced[student] = true;
        placedSeats.push_back(seat);
    }

    // Sanity check: no seat used twice, and no university over its seat count.
    if (!seats.countSeatsLeft(placedSeats).valid)
        return AdminMatchResult();

    // Everyone who was not placed (this includes students the matcher never heard of,
    // because they had no eligible university).
    for (int s = 0; s < students.size(); ++s)
    {
        if (!isPlaced[s])
            result.unplaced.push_back(students[s]);
    }

    result.valid = true;
    return result;
}

} // namespace

// Copies the lists first, then builds the scorer and the seat table FROM THE COPIES
// (the members are declared in this order, so students and universities exist before
// the scorer and seat table are built). Both therefore use the same university list.
AdminAllocator::AdminAllocator(const Vector<AdminStudent> &studentList,
                               const Vector<AdminUniversity> &universityList,
                               const Vector<AdminSeatCount> &adminSeatCounts,
                               bool allowUnranked,
                               const ScoringWeights &weights,
                               int defaultSeats)
    : students(studentList),
      universities(universityList),
      scorer(students, universities, allowUnranked, weights),
      seats(universities, adminSeatCounts, defaultSeats)
{
}

// Usable only if the seat table built properly and valid sliders were applied.
bool AdminAllocator::valid() const
{
    return seats.valid() && scorer.weightsApplied();
}

// Passes the sliders to the scorer, which only redoes the blending.
bool AdminAllocator::setWeights(const ScoringWeights &weights)
{
    return scorer.setWeights(weights);
}

// Option 1: build the graph from the seat table's lines, run Hopcroft-Karp (which
// maximises the number of students placed and ignores scores), then build the result.
AdminMatchResult AdminAllocator::runMostPlaced() const
{
    if (!valid())
        return AdminMatchResult();

    BipartiteGraph<int, int> graph;
    addEdgesToGraph(seats.buildEdges(scorer.pairs()), 0.0, graph);
    Vector<Pair<int, int>> matched = graph.get_max_cardinality_matching();
    return buildResult(students, universities, seats, scorer, matched);
}

// Option 2: the same, but with the Hungarian algorithm, which maximises the total
// score. The bonus added to every score is larger than the scores of all students
// added together (each score is at most 1), so placing one more student always beats
// any gain in scores. Scores then only decide between ways of placing the same number
// of students. The bonus also keeps every edge weight above 0, which the Hungarian
// code needs to tell real edges from its padding.
AdminMatchResult AdminAllocator::runBestFit() const
{
    if (!valid())
        return AdminMatchResult();

    double bonus = students.size() + 1.0;
    BipartiteGraph<int, int> graph;
    addEdgesToGraph(seats.buildEdges(scorer.pairs()), bonus, graph);
    Vector<Pair<int, int>> matched = graph.get_max_weight_matching();
    return buildResult(students, universities, seats, scorer, matched);
}

// Option 4: the same as option 2 but with NO bonus, so the Hungarian algorithm maximises the
// total score and nothing else. This is quality first: it may leave a student unplaced, even
// with a seat free, when that gives a higher total. Note that the Hungarian code treats a line
// with a score of exactly 0 as padding, so a student whose only match scores 0 is left
// unplaced here (placing them would add nothing to the total anyway).
AdminMatchResult AdminAllocator::runHighestScore() const
{
    if (!valid())
        return AdminMatchResult();

    BipartiteGraph<int, int> graph;
    addEdgesToGraph(seats.buildEdges(scorer.pairs()), 0.0, graph);
    Vector<Pair<int, int>> matched = graph.get_max_weight_matching();
    return buildResult(students, universities, seats, scorer, matched);
}

// Option 3: one Heap per university holds that university's eligible students, ordered by
// fit score (highest on top). The heap is a good fit because we usually need only the top
// few, and popping gives the next best candidate each time, like a waiting list. PriorityItem
// (from heap.hpp) breaks ties for us: on equal scores the smaller "seq" comes first, and we
// use the student's position as seq, so the lower student number wins a tie.
AdminShortlistResult AdminAllocator::runTopCandidates(int maxPerUniversity) const
{
    if (!valid())
        return AdminShortlistResult();

    AdminShortlistResult result;
    int universityCount = universities.size();
    const Vector<PairResult> &pairs = scorer.pairs();

    // Fill the heaps: every eligible pair goes into the heap of its university.
    Vector<Heap<PriorityItem>> heaps(universityCount, Heap<PriorityItem>());
    for (int i = 0; i < pairs.size(); ++i)
    {
        const PairResult &pair = pairs[i];
        if (pair.eligible())
            heaps[pair.universityIndex].push(PriorityItem(pair.studentIndex, pair.score, pair.studentIndex));
    }

    Vector<int> pairOf = buildPairLookup(pairs, students.size(), universityCount);

    // Build one shortlist per university, even if nobody is eligible there.
    for (int u = 0; u < universityCount; ++u)
    {
        AdminUniversityShortlist section;
        section.slug = universities[u].slug;
        section.name = universities[u].name;
        section.totalSeats = seats.universitySeats(u).totalSeats;
        section.seatsAssumed = seats.universitySeats(u).assumed;
        section.eligibleCount = heaps[u].size();

        // How many to list: everyone, or at most maxPerUniversity.
        int listed = heaps[u].size();
        if (maxPerUniversity > 0 && maxPerUniversity < listed)
            listed = maxPerUniversity;

        // Pop the best candidate again and again; each pop gives the next best.
        for (int rank = 1; rank <= listed; ++rank)
        {
            PriorityItem best;
            heaps[u].pop(best);
            const PairResult &pair = pairs[pairOf[best.id * universityCount + u]];

            AdminCandidate candidate;
            candidate.rank = rank;
            candidate.student = students[best.id];
            candidate.score = best.score;
            candidate.minGpaUnknown = pair.minGpaUnknown;
            candidate.statementMissing = pair.statementMissing;
            section.candidates.push_back(candidate);
        }
        result.byUniversity.push_back(section);
    }

    result.valid = true;
    return result;
}