#pragma once

// student_service.hpp
// Manages each student's academic profile, eligibility intake, and budget.
// Data structure: HashMap<email, StudentRecord> for O(1) lookups.

#include <string>
#include "hashmap.hpp"
#include "models/student_profile.hpp"
#include "repo/student_repo.hpp"

class StudentService {
public:
    explicit StudentService(StudentRepo& repo);

    // Load all students from SQLite into the HashMap at startup.
    void loadAll();

    // Find one student by lowercase AUC email. Returns nullptr if not saved yet.
    StudentRecord* find(const std::string& email);
    const StudentRecord* find(const std::string& email) const;

    // Validate, persist to SQLite, then update the in-memory HashMap.
    bool save(StudentRecord record, const std::string& now, std::string& error);

    // Evaluate IPSO's UG/GR eligibility rules on a student record.
    // checkEligibilityReason returns "" if eligible, otherwise the first failing rule.
    static std::string checkEligibilityReason(const StudentRecord& record);
    static StudentEligibilityOutcome evaluateEligibility(const StudentRecord& record);

private:
    StudentRepo& repo;
    HashMap<std::string, StudentRecord> students;
};
