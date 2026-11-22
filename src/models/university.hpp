
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
    
    // Minimum GPA required by the host university.
    // We use double because GPAs can contain decimals.
    double minGpa;

    // Stores the available exchange semesters as JSON text.
    std::string availableTermsJson;

    // Stores the eligible majors as JSON text.
    std::string eligibleMajorsJson;

    // Stores the official course catalog website link.
    std::string courseCatalogUrl;
    
    
};
