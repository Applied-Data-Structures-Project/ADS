#pragma once


#include <string>
#include "vector.hpp"

// One entry in the admin's list of seat counts, e.g. {"uva", 2}.
struct AdminSeatCount
{
    std::string slug;  // which university
    int seats = 0;     // how many places it has
};

// One physical seat, e.g. "uva #2".
struct AdminSeatSlot
{
    int universityIndex = 0;  // position of its university in the university list
    int seatNumber = 0;       // 1-based number within its university
    std::string label;        // e.g. "uva #2"
};

// The seat summary for one university.
struct AdminUniversitySeats
{
    int universityIndex = 0;
    int totalSeats = 0;
    int firstSeatIndex = -1;  // index of its first seat in the seat list, -1 if it has none
    bool assumed = false;     // true if the default seat count was used (admin did not set it)
};

// One line for the matcher: this student may take this seat, with this score.
struct AdminSeatEdge
{
    int studentIndex = 0;
    int seatIndex = 0;
    double score = 0.0;
};

// Seats used and left at one university after a matching run.
struct AdminUniversityUsage
{
    int universityIndex = 0;
    int totalSeats = 0;
    int filled = 0;
    int left = 0;
};

// The result of counting seats left. If valid is false the numbers must not be used.
struct AdminSeatUsage
{
    Vector<AdminUniversityUsage> usage;  // one entry per university
    bool valid = false;
};