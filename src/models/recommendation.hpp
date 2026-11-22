#pragma once

// recommendation.hpp
// One ranked university recommendation produced by RecommenderService using Heap<T>.
// Plain struct (no SQLite, no Crow, no STL containers).

#include <string>

struct UniversityRecommendation {
    std::string slug;
    std::string name;
    std::string country;
    std::string city;
    std::string region;

    int fitScore = 0;                  // 0..100 weighted fit score
    bool eligible = false;             // meets student level, host GPA, and major scope
    bool provisional = false;          // GR first semester without GPA yet
    std::string eligibilityReason;     // "" when eligible, otherwise first failing host/AUC rule
    double minGpaHost = -1.0;          // -1 when unverified
    int estimatedSemesterCostUsd = 0;  // estimated one-semester cost in USD
    bool withinBudget = false;         // true when student budget > 0 and cost <= budget
    std::string deadline;              // ISO YYYY-MM-DD deadline (for Min-Heap ordering)
    std::string matchSummary;          // concise reason for the score
};
