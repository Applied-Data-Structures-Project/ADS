#pragma once

// discovery.hpp
// Plain structs for saved universities and student notifications.
// No SQLite, no Crow, no STL containers.

#include <string>

struct SavedUniversity {
    std::string studentEmail;
    std::string universitySlug;
    std::string savedAt;
};

struct Notification {
    int id = 0;
    std::string studentEmail;
    std::string message;
    std::string createdAt;
    bool read = false;
};
