#pragma once
// Admin seat table: turns "university X has N places" into N numbered seats.


#include "vector.hpp"
#include "models/admin_models.hpp"
#include "models/admin_seat_models.hpp"
#include "services/admin_scoring.hpp"

// Seats given to a university the admin has not set. Change this one number to change the default.
const int DEFAULT_SEATS_PER_UNIVERSITY = 2;

class AdminSeatTable
{
public:

    AdminSeatTable(const Vector<AdminUniversity> &universities,
                   const Vector<AdminSeatCount> &adminCounts,
                   int defaultSeats = DEFAULT_SEATS_PER_UNIVERSITY);

    bool valid() const;          // true if the table is usable. Check this first.
    int seatCount() const;       // total seats across all universities
    int universityCount() const; // number of universities the table was built from

    // Details of one seat / one university.
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