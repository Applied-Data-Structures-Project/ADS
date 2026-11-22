
#pragma once

#include <string>

#include "vector.hpp"
#include "hashmap.hpp"
#include "models/university.hpp"
#include "repo/university_repo.hpp"

using namespace std;

// This class manages searching and finding universities.
class UniversityService {

private:
    UniversityRepo& repo;  // Reads university data from SQLite

    // Stores all universities in our Vector.
    Vector<University> universities;

    // Stores each university slug and its position in the Vector.
    HashMap<string, int> universityIndex;

public:
    // Constructor: receives the university repository.
    UniversityService(UniversityRepo& repository);

    // Loads all universities and creates the HashMap.
    bool loadAll();

    // Returns the number of universities.
    int count();

    // Finds one university using its unique slug.
    const University* find(string slug);

    // Searches universities by name.
    Vector<University> search(string name, string country, string region);

};
