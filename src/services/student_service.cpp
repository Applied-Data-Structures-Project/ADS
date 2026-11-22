// student_service.cpp
// Validation, eligibility evaluation, and HashMap storage for student profiles.

#include "services/student_service.hpp"

#include <cctype>
#include <cmath>

using namespace std;

static string trim(const string& text) {
    size_t start = 0;
    while (start < text.size() && isspace(static_cast<unsigned char>(text[start]))) start++;
    size_t end = text.size();
    while (end > start && isspace(static_cast<unsigned char>(text[end - 1]))) end--;
    return text.substr(start, end - start);
}

static string lower(string text) {
    for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return text;
}

static bool endsWith(const string& text, const string& suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

StudentService::StudentService(StudentRepo& repo) : repo(repo) {}

void StudentService::loadAll() {
    Vector<StudentRecord> rows = repo.loadStudents();
    for (int i = 0; i < rows.size(); i++) {
        string key = lower(rows[i].email);
        rows[i].email = key;
        students.insert(key, rows[i]);
    }
}

StudentRecord* StudentService::find(const string& email) {
    return students.get(lower(email));
}

const StudentRecord* StudentService::find(const string& email) const {
    return students.get(lower(email));
}

bool StudentService::save(StudentRecord record, const string& now, string& error) {
    record.email = lower(trim(record.email));
    if (record.email.empty() || !endsWith(record.email, "@aucegypt.edu")) {
        error = "Sign in with your AUC account (@aucegypt.edu).";
        return false;
    }

    record.name = trim(record.name);
    record.aucId = trim(record.aucId);
    record.phone = trim(record.phone);
    record.major = trim(record.major);

    if (!record.level.empty() && record.level != "UG" && record.level != "GR") {
        error = "Choose undergraduate (UG) or graduate (GR).";
        return false;
    }

    if (record.level == "UG") {
        record.firstSemesterNoGpa = false;
    }

    if (record.hasGpa && (!isfinite(record.gpa) || record.gpa < 0.0 || record.gpa > 4.0)) {
        error = "Enter a GPA between 0.00 and 4.00.";
        return false;
    }

    if (record.hasCredits && (record.credits < 0 || record.credits > 300)) {
        error = "Enter completed credits as a whole number (0-300).";
        return false;
    }

    if (record.budget < 0) record.budget = 0;

    if (record.eligibilitySaved) {
        if (record.level != "UG" && record.level != "GR") {
            error = "Choose undergraduate or graduate.";
            return false;
        }
        if (record.major.size() < 2) {
            error = "Enter your major.";
            return false;
        }
        if (!record.hasCredits) {
            error = "Enter completed credits as a whole number (0-300).";
            return false;
        }
        bool needsGpa = !(record.level == "GR" && record.firstSemesterNoGpa);
        if (needsGpa && !record.hasGpa) {
            error = "Enter a GPA between 0.00 and 4.00.";
            return false;
        }
    }

    record.updatedAt = now;

    // Write to SQLite first so memory and disk stay in sync on failure.
    if (!repo.saveStudent(record)) {
        error = "Could not save your profile to the database.";
        return false;
    }

    students.insert(record.email, record);
    return true;
}

string StudentService::checkEligibilityReason(const StudentRecord& r) {
    if (!r.eligibilitySaved) {
        return "Complete your eligibility profile first.";
    }

    bool isUG = (r.level != "GR");
    double minGpa = isUG ? 2.8 : 3.2;
    bool provisionalGpa = (!isUG && r.firstSemesterNoGpa && !r.hasGpa);

    if (!provisionalGpa && (!r.hasGpa || !isfinite(r.gpa) || r.gpa < minGpa)) {
        return isUG ? "Your GPA is below the 2.8 minimum for UG students."
                    : "Your GPA is below the 3.2 minimum for GR students.";
    }
    if (r.academicProbation) {
        return "Students on academic probation are not eligible.";
    }
    if (r.disciplinaryProbation) {
        return "Students on disciplinary probation are not eligible.";
    }

    if (isUG) {
        if (!r.hasCredits || r.credits < 24) {
            return "You need at least 24 completed credits.";
        }
        if (!r.majorDeclared) {
            return "Only students with an officially declared major can apply.";
        }
        if (!r.rhet110) {
            return "Complete RHET 110/1010 and CORE 110/1010 first.";
        }
        if (!r.rhet120) {
            return "Complete RHET 120/1020 first.";
        }
        if (r.finalSemester) {
            return "Students in their last semester at AUC are not eligible.";
        }
    } else {
        if (!r.englishComplete) {
            return "Complete all required English courses before applying.";
        }
    }
    return "";
}

StudentEligibilityOutcome StudentService::evaluateEligibility(const StudentRecord& r) {
    StudentEligibilityOutcome out;
    if (!r.eligibilitySaved) return out;

    bool isUG = (r.level != "GR");
    bool provisionalGpa = (!isUG && r.firstSemesterNoGpa && !r.hasGpa);
    out.eligible = checkEligibilityReason(r).empty();
    out.provisional = out.eligible && provisionalGpa;
    return out;
}
