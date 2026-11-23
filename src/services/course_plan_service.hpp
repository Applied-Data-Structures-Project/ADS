#pragma once

// course_plan_service.hpp
// Manages each student's Draft List of Courses per host university.
// Data structures (no STL containers):
//   HashMap<"email|slug", CoursePlan>    O(1) course plan lookup and update
//   HashMap<email, Vector<slug>>         planned host universities per student
//   mergeSort (include/mergesort.hpp)    orders a student's plans by most recently updated

#include <string>
#include "hashmap.hpp"
#include "vector.hpp"
#include "models/course_plan.hpp"
#include "repo/course_plan_repo.hpp"

class CoursePlanService {
public:
    static constexpr int MAX_COURSES_PER_PLAN = 15;

    explicit CoursePlanService(CoursePlanRepo& repo);

    void loadAll();

    Vector<CoursePlan> plansOf(const std::string& email) const;
    const CoursePlan* find(const std::string& email, const std::string& universitySlug) const;

    bool savePlan(const CoursePlan& input, const std::string& now, std::string& error);
    bool deletePlan(const std::string& email, const std::string& universitySlug, std::string& error);

private:
    CoursePlanRepo& repo;

    HashMap<std::string, CoursePlan> plansByKey;
    HashMap<std::string, Vector<std::string>> slugsByStudent;

    static std::string makeKey(const std::string& email, const std::string& slug);
    static std::string trim(const std::string& s);
    static std::string lower(const std::string& s);
};
