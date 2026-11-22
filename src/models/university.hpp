
#pragma once

#include <string>

// This struct stores the information of one university.

struct University {
    std::string slug;    // Unique ID used to find the university
    std::string name;     // University name
    std::string country;       // Country where it is located
    std::string city;     // City where it is located
    std::string region;       // Region, such as Europe or East Asia
    std::string metadataJson;    // Extra information from the database
    
    // Minimum GPA required by the host university (-1 when unverified).
    double minGpa = -1.0;

    // Stores the available exchange semesters as JSON text.
    std::string availableTermsJson;

    // Stores the eligible majors as JSON text.
    std::string eligibleMajorsJson;

    // Stores the official course catalog website link.
    std::string courseCatalogUrl;

    // Additional verified fields used by the C++ recommender.
    std::string agreementStatus;       // "active", "check_slots", "letter_of_intent"
    std::string studentLevelsJson;     // e.g. ["UG", "GR"]
    std::string excludedMajorsJson;    // e.g. ["Medicine"]
    std::string tuitionModel;          // e.g. "auc_exchange_waiver"
    double livingCostUsdEst = -1.0;    // estimated monthly living cost in USD (-1 if unknown)
};
