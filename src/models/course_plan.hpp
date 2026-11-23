#pragma once

// course_plan.hpp
// Domain models for a student's Draft List of Courses for Study Abroad.
// Strictly free of Crow, SQLite, and STL containers.

#include <string>
#include "vector.hpp"

struct PlannedCourse {
    std::string hostCourseCode;
    std::string hostCourseTitle;
    std::string aucEquivalent;
    std::string effectiveTerm;
    std::string source = "manual"; // "banner" or "manual"
};

struct CoursePlan {
    std::string studentEmail;
    std::string universitySlug;
    std::string universityName;
    std::string bannerInstCode;
    std::string bannerInstName;
    std::string studyTerm = "Fall 2027";
    Vector<PlannedCourse> courses;
    std::string updatedAt;
};
