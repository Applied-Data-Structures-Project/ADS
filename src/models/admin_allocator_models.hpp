#pragma once


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

// One candidate in a university's shortlist (option 3).
struct AdminCandidate
{
    int rank = 0;                     // 1 = best fit at this university
    AdminStudent student;             // a COPY of the student
    double score = 0.0;               // the fit score of this student at this university
    bool minGpaUnknown = false;       // eligible, but the university's minimum GPA is unknown
    bool statementMissing = false;    // the student has no statement rating yet
};

// The shortlist for one university.
struct AdminUniversityShortlist
{
    std::string slug;            // e.g. "uva"
    std::string name;            // full display name
    int totalSeats = 0;          // so the admin can see where the seats run out
    bool seatsAssumed = false;   // true if totalSeats is the default, not set by the admin
    int eligibleCount = 0;       // how many students are eligible here in total (the list may be shorter)
    Vector<AdminCandidate> candidates;   // best first
};

// The result of option 3: a ranked shortlist for every university.
struct AdminShortlistResult
{
    Vector<AdminUniversityShortlist> byUniversity;  // one entry per university, even with no candidates
    bool valid = false;                             // false if the allocator was not usable
};