#pragma once

// application.hpp
// Group 3 - Malak
//
// The data for a student's study abroad application.
// Each struct here matches one table in 002_application.sql:
//   Application         -> applications
//   ApplicationDocument -> application_documents
//   LetterRequest       -> letters
//
// These are just plain containers for the data. All the checking
// (eligibility, preferences, submit rules) happens in the services.

#include <string>

using std::string;

// One student's application (one row in `applications`).
// Most fields start empty because students can save a half-finished draft.
struct Application {
    string studentEmail;

    // Personal info, same as IPSO's form
    string firstName;
    string surname;
    string dateOfBirth;        // MM/DD/YYYY
    string nationality;
    string secondNationality;  // optional
    string gender;             // "Male" or "Female"
    string phone;              // with country code
    string aucId;              // 9 digits
    double gpa = 0.0;
    string level;              // "UG" or "GR"
    string major;
    string rhet1020;           // "Yes", "Currently Enrolled" or "Not Applicable"
    string expectedGraduation;
    bool accommodation = false;
    string scholarship;        // "None", "Tomorrow's Leaders" or "Other"
    string programType;        // "Exchange" or "Direct Enrollment"

    // The 3 ranked choices, as university slugs (e.g. "aarhus-university").
    // pref[0] is the first choice. An empty string means not chosen yet.
    string pref[3];

    // Parent or guardian
    string guardianName;
    string guardianRelation;
    string guardianPhone;
    string guardianEmail;
    string guardianLanguage;   // optional

    // Questionnaire answers
    string qPreparedness;
    string qGoals;             // 300 to 500 words
    string travelExperience;   // "Yes, Study Abroad", "Yes, Non-Study Abroad", "No" or "Other"
    string qAdaptability;      // at least 300 words
    string qTeamwork;
    string qReflection;
    string qFinance;

    // Extra info the admin side uses for matching
    int budget = 0;
    bool prior = false;        // true if they studied abroad before

    // Submitting and review
    bool declaration = false;
    string status = "Draft";   // Draft, Submitted, In Review, Verified or Ineligible
    int goodStanding = -1;     // -1 not checked yet, 0 no, 1 yes (set by IPSO)
    string submittedAt;
    string createdAt;
    string updatedAt;
};

// One uploaded PDF (one row in `application_documents`).
// Uploading the same type again replaces the old file.
struct ApplicationDocument {
    string studentEmail;
    string docType;            // "courses", "transcript", "cv" or "passport"
    string filePath;
    string uploadedAt;
};

// One recommendation letter request (one row in `letters`).
struct LetterRequest {
    int id = -1;               // -1 until it's saved in the database
    string studentEmail;
    string doctorName;
    string doctorEmail;        // must be @aucegypt.edu
    string status = "Pending"; // Pending, Received or Declined
    string letterText;
    string requestedAt;
    string answeredAt;
};