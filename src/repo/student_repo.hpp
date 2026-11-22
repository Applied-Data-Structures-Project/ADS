#pragma once

// student_repo.hpp
// The only file that runs SQL on the `students` table.

#include "sqlite3.h"
#include "vector.hpp"
#include "models/student_profile.hpp"

class StudentRepo {
public:
    explicit StudentRepo(sqlite3* db);

    Vector<StudentRecord> loadStudents();
    bool saveStudent(const StudentRecord& s);

private:
    sqlite3* db;
};
