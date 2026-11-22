#pragma once
// Admin seat table: turns "university X has N places" into N numbered seats.
//
// WHY THIS EXISTS
// The matcher (BipartiteGraph) puts ONE student into ONE item, and an item can be
// used only once. A university with 2 places must therefore become 2 separate
// items, "uva #1" and "uva #2". This class creates those seats, produces the lines
// ("this student may take this seat, with this score") that feed the matcher, and
// translates the matcher's answer back from a seat to its university.
//
// WHY EVERY UNIVERSITY KEEPS ALL ITS SEATS
// Even a university nobody applied to keeps its seats. The admin needs to see real
// seat counts and how many are left, and students who miss their choices can later
// be offered the seats that remain. For the same reason, "seats left" is worked out
// HERE from our own seat list, not from the matcher: a seat nobody can take never
// appears in the matcher's graph.
//
// WHY A CLASS
// The seat list has to be remembered between calls (build once, then look things up
// and count seats left), so it lives in an object. The table is never changed after
// it is built, so every matching option can share the same one.
//
// IMPORTANT: this table and the AdminFitScorer must be built from the SAME
// university list in the SAME order. Each pair points at its university by position.
//
// This file must not include Crow (HTTP) or SQLite, so it can be tested alone.

#include "vector.hpp"
#include "models/admin_models.hpp"
#include "models/admin_seat_models.hpp"
#include "services/admin_scoring.hpp"

// Seats given to a university the admin has not set. Change this one number to change the default.
const int DEFAULT_SEATS_PER_UNIVERSITY = 2;

class AdminSeatTable
{
public:
    // Builds every seat once. Copies what it needs and keeps no link to the inputs.
    // If a university in the list ends up with a NEGATIVE seat count (or the default
    // is negative) the table is INVALID: valid() is false and it holds no seats.
    AdminSeatTable(const Vector<AdminUniversity> &universities,
                   const Vector<AdminSeatCount> &adminCounts,
                   int defaultSeats = DEFAULT_SEATS_PER_UNIVERSITY);

    bool valid() const;          // true if the table is usable. Check this first.
    int seatCount() const;       // total seats across all universities
    int universityCount() const; // number of universities the table was built from

    // Details of one seat / one university. Throw std::out_of_range for a bad index.
    const AdminSeatSlot &slot(int seatIndex) const;
    const AdminUniversitySeats &universitySeats(int universityIndex) const;

    // The way back: which university does this seat belong to? -1 if no such seat.
    int universityOfSeat(int seatIndex) const;

    // The matcher's input: one line per (eligible pair, seat of that university).
    // Empty if the table is invalid. Pairs pointing at an unknown university are skipped.
    Vector<AdminSeatEdge> buildEdges(const Vector<PairResult> &pairs) const;

    // After a matching run: how many seats are filled and left at each university.
    // placedSeatIndices holds one seat number per placed student. The result is
    // invalid if the table is invalid, a seat number does not exist, or a seat repeats.
    AdminSeatUsage countSeatsLeft(const Vector<int> &placedSeatIndices) const;

private:
    Vector<AdminSeatSlot> slots;        // every seat, numbered 0, 1, 2... across all universities
    Vector<AdminUniversitySeats> info;  // one entry per university, same order as the university list
    bool isValid;                       // false if the admin's input was unusable
};