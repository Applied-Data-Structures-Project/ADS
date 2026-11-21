#pragma once
// Plain data structs for the result of one matching run (admin side).
// No logic here, only data. A result is a SNAPSHOT: it holds copies of the real
// data (students, university names), not positions in lists, so it stays correct
// even if the live student list changes afterwards (for example in a saved run).

#include <string>
#include "vector.hpp"
#include "models/admin_models.hpp"

// One student who got a seat.
struct AdminPlacedStudent
{
    AdminStudent student;    // a COPY of the student
    std::string seatLabel;   // e.g. "uva #2"
    double score = 0.0;      // the fit score of this student at this university
};

// Everything about one university in one run.
struct AdminUniversityResult
{
    std::string slug;            // e.g. "uva"
    std::string name;            // full display name
    int totalSeats = 0;
    bool seatsAssumed = false;   // true if totalSeats is the default, not set by the admin
    Vector<AdminPlacedStudent> placed;   // students placed here (seats left = totalSeats - placed.size())
};

// The result of one run of one matching option.
struct AdminMatchResult
{
    Vector<AdminUniversityResult> byUniversity;  // one entry per university, even with nobody placed
    Vector<AdminStudent> unplaced;               // students who got no seat
    bool valid = false;                          // false if the run could not be done
};