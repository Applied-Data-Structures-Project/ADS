#pragma once

// student_profile.hpp
// One student's academic profile, eligibility intake fields, and semester budget.
// Plain struct (no SQLite, no Crow, no STL containers).

#include <string>

struct StudentRecord {
    std::string email;
    std::string name;
    std::string aucId;
    std::string phone;
    std::string level;              // "" until chosen, then "UG" or "GR"
    std::string major;

    double gpa = 0.0;
    bool hasGpa = false;            // false when GPA is null (e.g. GR first semester)

    int credits = 0;
    bool hasCredits = false;

    bool majorDeclared = false;
    bool rhet110 = false;
    bool rhet120 = false;
    bool finalSemester = false;
    bool academicProbation = false;
    bool disciplinaryProbation = false;
    bool englishComplete = false;
    bool firstSemesterNoGpa = false;

    bool eligibilitySaved = false;  // true once the student submits /profile

    int budget = 0;                 // per-semester USD budget (used on /me and /compare)
    bool hasBudget = false;

    std::string updatedAt;
};

struct StudentEligibilityOutcome {
    bool eligible = false;
    bool provisional = false;
};
