#pragma once
// Admin allocator: runs the matching options and returns who got which university.


#include "vector.hpp"
#include "models/admin_models.hpp"
#include "models/admin_seat_models.hpp"
#include "models/admin_allocator_models.hpp"
#include "services/admin_scoring.hpp"
#include "services/admin_seats.hpp"

class AdminAllocator
{
public:
    // Copies the students and universities, then builds the scorer and the seat
    // table from those copies. If the sliders are invalid, or a seat count is
    // negative, the allocator is built but valid() is false and every run returns
    // an invalid (empty) result.
    AdminAllocator(const Vector<AdminStudent> &studentList,
                   const Vector<AdminUniversity> &universityList,
                   const Vector<AdminSeatCount> &adminSeatCounts,
                   bool allowUnranked,
                   const ScoringWeights &weights,
                   int defaultSeats = DEFAULT_SEATS_PER_UNIVERSITY);

    // True when the seat table is usable and valid sliders have been applied.
    bool valid() const;

    // Moves the sliders. Only the scores are recalculated (the checks are not redone).
    // Returns false and changes nothing if the sliders are invalid.
    bool setWeights(const ScoringWeights &weights);

    // Option 1: the most students placed. Scores are ignored.
    AdminMatchResult runMostPlaced() const;

    // Option 2: the most students placed, then the best total fit.
    AdminMatchResult runBestFit() const;

    // Option 4: the highest total score, even if fewer students are placed.
    // (Compare option 2, which places the most students first.)
    AdminMatchResult runHighestScore() const;

    // Option 3: a ranked shortlist of eligible students for every university.
    // maxPerUniversity limits how many are listed per university (0 or less = everyone).
    AdminShortlistResult runTopCandidates(int maxPerUniversity) const;

private:
    Vector<AdminStudent> students;          // our own copy, so positions can never shift
    Vector<AdminUniversity> universities;   // our own copy, so positions can never shift
    AdminFitScorer scorer;                  // built from the copies above
    AdminSeatTable seats;                   // built from the same universities copy
};