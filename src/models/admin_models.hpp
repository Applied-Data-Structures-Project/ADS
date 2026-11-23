#pragma once

#include <string>
#include "vector.hpp"

// One student who wants to go on exchange.
struct AdminStudent
{
    int id = 0;
    std::string name;
    double gpa = 0.0;            // e.g. 3.78
    std::string majorCode;       // e.g. "CSCE"
    std::string semester;        // the semester the student wants: "Fall" or "Spring"

    // The student's 3 ranked choices, as university slugs.
    // prefs[0] = first choice, prefs[1] = second, prefs[2] = third.
    // An empty string means "no choice made for this rank".
    std::string prefs[3];

    double statementRating = 0.0;     // rating out of 10 for the personal statement
    bool hasStatementRating = false;  // false = the statement has not been rated yet
    bool studiedAbroadBefore = false; // used by the fairness factor
    std::string status = "Verified";  // application status, e.g. "Verified" or "Ineligible"
};

// One partner university.
struct AdminUniversity
{
    std::string slug;  // short unique id, e.g. "uva"
    std::string name;  // full display name

    bool minGpaKnown = false;  // false = we do not know the university's minimum GPA
    double minGpa = 0.0;       // only meaningful when minGpaKnown is true

    // Major codes the university accepts. Empty = unknown. May contain "ALL".
    Vector<std::string> acceptedMajors;

    // Semesters the university offers ("Fall", "Spring"). Empty = unknown.
    Vector<std::string> semesters;
};

// The admin's slider values. Each one must be >= 0 (checked in the scoring service).
struct ScoringWeights
{
    double preference = 0.0;
    double gpa = 0.0;
    double statement = 0.0;
    double fairness = 0.0;
};
