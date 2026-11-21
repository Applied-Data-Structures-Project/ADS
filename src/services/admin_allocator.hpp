#pragma once
// Admin allocator: runs the matching options and returns who got which university.
//
// WHAT IT DOES
// Given the students, the universities, the admin's seat counts and the slider
// weights, it builds the fit scores (AdminFitScorer) and the numbered seats
// (AdminSeatTable), then runs a matching option on the BipartiteGraph and turns
// the answer into an AdminMatchResult.
//
// WHY IT BUILDS THE SCORER AND THE SEAT TABLE ITSELF
// Each score pair points at its university by POSITION in the university list.
// The scorer and the seat table must therefore be built from the SAME list in the
// SAME order, or lines would land on the wrong seats with no error. The allocator
// keeps its own copy of the lists and builds both from that copy, so they can never
// disagree.
//
// WHY A RUN RETURNS ITS RESULT INSTEAD OF STORING IT
// The result is a snapshot (copies of the real students and university names).
// Because nothing is stored, option 1 and option 2 can be run on the same allocator
// and their results compared side by side.
//
// THE TWO OPTIONS
//   runMostPlaced(): places as many students as possible (Hopcroft-Karp). Scores
//                    are ignored; which students are chosen depends on input order.
//   runBestFit():    places as many students as possible AND, among those ways,
//                    picks the highest total score (Hungarian). A large bonus is added
//                    to every score so that "more students placed" always beats
//                    "higher scores". Without it the Hungarian algorithm could leave a
//                    student out just to get a higher total.
//
// This file must not include Crow (HTTP) or SQLite, so it can be tested alone.

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

private:
    Vector<AdminStudent> students;          // our own copy, so positions can never shift
    Vector<AdminUniversity> universities;   // our own copy, so positions can never shift
    AdminFitScorer scorer;                  // built from the copies above
    AdminSeatTable seats;                   // built from the same universities copy
};