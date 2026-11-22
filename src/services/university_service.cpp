
#include "services/university_service.hpp"

#include <cctype> // Needed to convert letters to lowercase.


using namespace std;

// Constructor: connect the service to the repository.
UniversityService::UniversityService(UniversityRepo& repository)
    : repo(repository) {
}

// Load universities and build the HashMap.
bool UniversityService::loadAll() {

    // Read all universities from SQLite.
    if (!repo.loadUniversities(universities)) {
        return false;
    }

    // Go through each university in the Vector.
    for (int i = 0; i < universities.size(); i++) {

        // Get the university's unique slug.
        string slug = universities[i].slug;

        // Store the slug as the key and vector position as the value.
        universityIndex.insert(slug, i);
    }

    return true;
}

// Return the total number of universities.
int UniversityService::count() {
    return universities.size();
}

const Vector<University>& UniversityService::all() const {
    return universities;
}

// Find one university using the HashMap.
const University* UniversityService::find(string slug) {

    // Get the position of the university in the vector.
    int* index = universityIndex.get(slug);

    // Return nullptr if the university was not found.
    if (index == nullptr) {
        return nullptr;
    }

    // Return the university stored at that position.
    return &universities[*index];
}

static string toLowercase(string text) {

    // Use size_t because string length is an unsigned number.
    for (size_t i = 0; i < text.length(); i++) {
        text[i] = tolower((unsigned char)text[i]);
    }

    return text;
}


// Search university names.
Vector<University> UniversityService::search(string name, string country, string region) {

    // Store the universities that match the search.
    Vector<University> results;

    
    // Convert the search inputs to lowercase.
       // This allows students to search without matching capital letters.
       name = toLowercase(name);
       country = toLowercase(country);
       region = toLowercase(region);
    
    
    // Go through all universities one by one.
    for (int i = 0; i < universities.size(); i++) {
        
        // Get the university we are currently checking.
               University u = universities[i];
        
        
        // Compare lowercase values to ignore capitalization.
               string universityName = toLowercase(u.name);
               string universityCountry = toLowercase(u.country);
               string universityRegion = toLowercase(u.region);

        // Check whether the university name contains the search text.
               // If name is empty, every university matches this condition.
               bool nameMatches =
                   universityName.find(name) != string::npos;

               // Check whether the country matches the selected country.
               // If country is empty, we include universities from all countries.
               bool countryMatches =
                   country == "" || universityCountry == country;

               // Check whether the region matches the selected region.
               // If region is empty, we include universities from all regions.
               bool regionMatches =
                   region == "" || universityRegion == region;

        
        // Only include universities that satisfy all three conditions.
               if (nameMatches && countryMatches && regionMatches) {
                   results.push_back(u);
        }
    }

    // Return all matching universities.
    return results;
}
