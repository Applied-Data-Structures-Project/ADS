

#pragma once

#include "sqlite3.h"
#include "vector.hpp"
#include "models/university.hpp"

// This class reads university information from the database.
class UniversityRepo {

private:
    sqlite3* db;  // Connection to our SQLite database

public:
    
    // Constructor: receives an already opened database.
    UniversityRepo(sqlite3* database);

    // Loads universities from SQLite into our Vector.
    // Returns true if successful and false if something goes wrong.
    bool loadUniversities(Vector<University>& result);

};

