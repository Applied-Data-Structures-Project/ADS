#pragma once

// test_helpers.hpp
// Small helpers shared by the Group 3 tests:
//   CHECK(...)      - prints PASS or FAIL for one condition and counts the failures
//   openTestDb()    - a fresh database in memory with our tables, so tests never touch db/app.db

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include "sqlite3.h"

static int failures = 0;

#define CHECK(condition, name)                                        \
    do {                                                              \
        if (condition) {                                              \
            std::printf("  PASS  %s\n", name);                        \
        } else {                                                      \
            std::printf("  FAIL  %s  (line %d)\n", name, __LINE__);   \
            failures++;                                               \
        }                                                             \
    } while (0)

// The project folder, worked out from where this file is (".../tests/test_helpers.hpp").
static std::string projectRoot() {
    std::string here = __FILE__;
    std::size_t pos = here.rfind("tests/");
    if (pos == std::string::npos || pos == 0) return ".";   // built from inside the project folder
    return here.substr(0, pos - 1);                          // everything before "/tests/"
}

static sqlite3* openTestDb() {
    sqlite3* db = nullptr;
    sqlite3_open(":memory:", &db);
    sqlite3_exec(db, "CREATE TABLE students(email TEXT PRIMARY KEY);"
                     "CREATE TABLE universities(slug TEXT PRIMARY KEY);", nullptr, nullptr, nullptr);

    std::ifstream file(projectRoot() + "/db/migrations/002_application.sql");
    std::stringstream sql;
    sql << file.rdbuf();
    if (sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::printf("  Could not create the tables: %s\n", sqlite3_errmsg(db));
        failures++;
    }
    return db;
}
