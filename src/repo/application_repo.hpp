#pragma once

// application_repo.hpp

// It loads rows into our structs when the server starts,
// and saves one row back whenever something changes.
// No checking or sorting here; that happens in the services.

#include <string>
#include "sqlite3.h"
#include "vector.hpp"
#include "models/application.hpp"

class ApplicationRepo {
public:
    // Uses a database connection that is already open.
    explicit ApplicationRepo(sqlite3* db);

    // Applications
    Vector<Application> loadApplications();
    bool saveApplication(const Application& a);

    // Uploaded documents
    Vector<ApplicationDocument> loadDocuments();
    bool saveDocument(const ApplicationDocument& d);
    bool deleteDocument(const std::string& email, const std::string& docType);

    // Recommendation letters
    Vector<LetterRequest> loadLetters();
    int insertLetter(const LetterRequest& l);   // returns the new id, or -1 on error
    bool updateLetter(const LetterRequest& l);

private:
    sqlite3* db;
};