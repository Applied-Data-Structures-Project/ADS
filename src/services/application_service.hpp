#pragma once

// application_service.hpp
// The rules of the study abroad application, and the data structures behind them.
// The repo only loads and saves; this file decides what is allowed.
// Data structures:
//   HashMap<string, Application>  email -> application        (find anyone in O(1))
//   HashMap<string, string>       "email|docType" -> file      (which PDFs are uploaded)
//   Queue<string>                 emails of submitted students (first come, first reviewed)
//   string pref[3]                the 3 ranked choices, inside each Application

#include <string>
#include "hashmap.hpp"
#include "queue.hpp"
#include "vector.hpp"
#include "models/application.hpp"
#include "repo/application_repo.hpp"

// IPSO's rules, kept in one place so they are easy to change each term.
struct ApplicationRules {
    double minGpaUG = 2.8;
    double minGpaGR = 3.2;
    int minCredits = 24;
    int goalsMinWords = 300;
    int goalsMaxWords = 500;
    int adaptMinWords = 300;
    int adaptMaxWords = 5000;
    std::string deadline = "2026-11-30T15:00:00";   // same format as the dates we save
};

// What the eligibility check needs to know about a student (comes from the profile).
struct StudentInfo {
    std::string email;
    std::string level;     // "UG" or "GR"
    bool declared = false;
    int credits = 0;
    double gpa = 0.0;
};
class ApplicationService {
public:
    ApplicationService(ApplicationRepo& repo, ApplicationRules rules = ApplicationRules());

    // fill the HashMaps and the Queue from the database.
    void loadAll();

    // Find one student's application. Returns nullptr if they haven't started one.
    Application* find(const std::string& email);

    // Eligibility: returns "" if the student may apply, otherwise the reason.
    std::string checkEligibility(const StudentInfo& s) const;

    // Save a draft. Refused (returns false + error) once the application is submitted.
    bool saveDraft(const Application& a, const std::string& now, std::string& error);

    // Preferences: returns "" if the 3 choices are fine, otherwise the problem.
    std::string checkPreferences(const Application& a) const;

    // Counts the words in an essay.
    static int wordCount(const std::string& text);

    // Documents
    bool uploadDocument(const std::string& email, const std::string& docType,
                        const std::string& filePath, const std::string& now, std::string& error);
    bool removeDocument(const std::string& email, const std::string& docType, std::string& error);
    bool hasDocument(const std::string& email, const std::string& docType) const;

    // Everything still missing before this student can submit (empty = ready).
    Vector<std::string> missing(const std::string& email, bool letterReceived) const;

    // Submit: checks everything, then locks the application and joins the review queue.
    bool submit(const std::string& email, bool letterReceived, const std::string& now,
                std::string& error);

    // The review queue (used by the admin side).
    int waitingForReview() const;
    std::string nextForReview() const;   // "" if nobody is waiting
    void finishReview();                 // removes the student at the front

private:
    ApplicationRepo& repo;
    ApplicationRules rules;
    HashMap<std::string, Application> applications;
    HashMap<std::string, std::string> documents;
    Queue<std::string> reviewQueue;

    static std::string docKey(const std::string& email, const std::string& docType);
};