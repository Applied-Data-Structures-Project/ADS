#pragma once

// course_plan_repo.hpp
// SQLite repository for `course_plans` and `course_plan_items`.

#include <string>
#include "sqlite3.h"
#include "vector.hpp"
#include "models/course_plan.hpp"

class CoursePlanRepo {
public:
    explicit CoursePlanRepo(sqlite3* db);

    Vector<CoursePlan> loadAll();
    bool upsertPlan(const CoursePlan& plan);
    bool deletePlan(const std::string& studentEmail, const std::string& universitySlug);

private:
    sqlite3* db;
};
