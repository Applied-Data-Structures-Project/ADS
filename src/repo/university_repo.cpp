
#include "repo/university_repo.hpp"

using namespace std;

// This function helps us read text from our SQLite database.
// SQLite returns text as a pointer, but we want to store it as a normal C++ string inside our University struct.
//
// We pass two things to the function:
// 1. statement: the SQL query that we are reading from.
// 2. column: the number of the column we want to read.
//
// For example, column 0 contains the university slug,and column 1 contains the university name.

static string getText(sqlite3_stmt* statement, int column) {

    // Get the text stored in the selected database column.
    const unsigned char* text =
        sqlite3_column_text(statement, column);

    // Some university fields may be empty in our database.
       // SQLite represents missing values using NULL.
       // If we find a NULL value, we return an empty strin so the program does not try to read a null pointer.
    if (text == nullptr) {
        return "";
    }
    // Convert the SQLite text into a C++ string.
    return (const char*)text;
}


// Constructor of the UniversityRepo class.
// The database connection is already opened by another part of the program. We receive that connection and store it so we can use it to run SQL queries.
// We do not open a new database here.
UniversityRepo::UniversityRepo(sqlite3* database) {
    db = database;
}

// This function loads all universities from SQLite.
// The result parameter is a reference to our Vector.
// This means we can fill the original Vector directly, instead of returning a separate copy.
// The function returns:
// true  -> if the universities were loaded successfully.
// false -> if there was a problem reading the database.

bool UniversityRepo::loadUniversities(Vector<University>& result) {

    // Before using the database, we check that the connection actually exists.
     // nullptr means the pointer does not point to a valid database connection.
    if (db == nullptr) {
        return false;
    }

    // This query selects the university information we need.
       // SELECT tells SQLite which columns to retrieve.
       // The first six columns contain the basic university details.
       // The next four columns contain academic requirements
       // and the university's course catalog link.
       // We will read the columns in exactly this order below.
    
    const char* sql =
    "SELECT slug, official_name, country, city, region, metadata_json, "
            "min_gpa_host, available_terms_json, eligible_majors_json, "
            "course_catalog_url "
            "FROM universities";

    // A statement is the SQL query after SQLite prepares it.
       // We start with nullptr because the statement has not been prepared yet.
    sqlite3_stmt* statement = nullptr;

    // Prepare the SQL query so SQLite can execute it.
       // db        -> the database connection.
       // sql       -> the query we want to execute.
       // -1        -> SQLite reads the full query string.
       // &statement -> stores the prepared statement.
       // nullptr   -> we do not need the unused SQL remainder.
       // SQLite returns a status code telling us whether preparing the query was successful.
    int status = sqlite3_prepare_v2(
        db, sql, -1, &statement, nullptr
    );

    // Stop if the query could not be prepared.
    if (status != SQLITE_OK) {
        return false;
    }

    // Store the universities here before returning them.
    Vector<University> loaded;

    // Read one university row at a time.
    while ((status = sqlite3_step(statement)) == SQLITE_ROW) {

        // Create a University object for the current row.
        University u;

        // Read the values from the database columns.
        u.slug = getText(statement, 0);
        u.name = getText(statement, 1);
        u.country = getText(statement, 2);
        u.city = getText(statement, 3);
        u.region = getText(statement, 4);
        u.metadataJson = getText(statement, 5);
        
        // Read the minimum GPA requirement.
        // If the GPA is missing, store -1.
        if (sqlite3_column_type(statement, 6) == SQLITE_NULL) {
            u.minGpa = -1;
        }
        else {
            u.minGpa = sqlite3_column_double(statement, 6);
        }

        // Read the available exchange semesters.
        u.availableTermsJson = getText(statement, 7);

        // Read the eligible majors.
        u.eligibleMajorsJson = getText(statement, 8);

        // Read the university's course catalog URL.
        u.courseCatalogUrl = getText(statement, 9);
        

        // Add the university to our Vector.
        loaded.push_back(u);
    }

    // Release the SQL statement after using it.
    sqlite3_finalize(statement);

    // SQLITE_DONE means SQLite reached the end of the query results successfully.
        // If the loop stopped for another reason, an error may have happened while reading.
        // In that case, we return false rather than giving the program incomplete university data.

    if (status != SQLITE_DONE) {
        return false;
    }

    // Copy the loaded universities into the result Vector.
    result = loaded;

    return true;
}

