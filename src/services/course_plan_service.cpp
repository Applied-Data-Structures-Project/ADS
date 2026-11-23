// course_plan_service.cpp
// In-memory HashMap + Vector store with SQLite persistence for Draft Course Lists.

#include "services/course_plan_service.hpp"
#include "mergesort.hpp"
#include <cctype>

using namespace std;

CoursePlanService::CoursePlanService(CoursePlanRepo& repo) : repo(repo) {}

string CoursePlanService::makeKey(const string& email, const string& slug) {
    return lower(trim(email)) + "|" + trim(slug);
}

string CoursePlanService::trim(const string& s) {
    size_t start = 0;
    while (start < s.size() && isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

string CoursePlanService::lower(const string& s) {
    string out = s;
    for (size_t i = 0; i < out.size(); i++) {
        out[i] = static_cast<char>(tolower(static_cast<unsigned char>(out[i])));
    }
    return out;
}

void CoursePlanService::loadAll() {
    Vector<CoursePlan> all = repo.loadAll();
    for (int i = 0; i < all.size(); i++) {
        CoursePlan p = all[i];
        p.studentEmail = lower(trim(p.studentEmail));
        p.universitySlug = trim(p.universitySlug);
        if (p.studentEmail.empty() || p.universitySlug.empty()) continue;

        string key = makeKey(p.studentEmail, p.universitySlug);
        bool existed = plansByKey.search(key);
        plansByKey.insert(key, p);

        if (!existed) {
            Vector<string>* list = slugsByStudent.get(p.studentEmail);
            if (!list) {
                Vector<string> fresh;
                fresh.push_back(p.universitySlug);
                slugsByStudent.insert(p.studentEmail, fresh);
            } else {
                list->push_back(p.universitySlug);
            }
        }
    }
}

Vector<CoursePlan> CoursePlanService::plansOf(const string& email) const {
    string normEmail = lower(trim(email));
    Vector<CoursePlan> out;
    const Vector<string>* slugs = slugsByStudent.get(normEmail);
    if (!slugs) return out;

    out.reserve(slugs->size());
    for (int i = 0; i < slugs->size(); i++) {
        const CoursePlan* p = plansByKey.get(makeKey(normEmail, (*slugs)[i]));
        if (p) out.push_back(*p);
    }

    // Order most recently updated first using our C++ stable mergeSort.
    mergeSort(out, [](const CoursePlan& a, const CoursePlan& b) {
        if (a.updatedAt != b.updatedAt) return a.updatedAt > b.updatedAt;
        return a.universitySlug < b.universitySlug;
    });
    return out;
}

const CoursePlan* CoursePlanService::find(const string& email, const string& universitySlug) const {
    return plansByKey.get(makeKey(email, universitySlug));
}

bool CoursePlanService::savePlan(const CoursePlan& input, const string& now, string& error) {
    CoursePlan clean;
    clean.studentEmail = lower(trim(input.studentEmail));
    clean.universitySlug = trim(input.universitySlug);
    clean.universityName = trim(input.universityName);
    clean.bannerInstCode = trim(input.bannerInstCode);
    clean.bannerInstName = trim(input.bannerInstName);
    clean.studyTerm = trim(input.studyTerm);
    if (clean.studyTerm.empty()) clean.studyTerm = "Fall 2027";
    clean.updatedAt = now;

    if (clean.studentEmail.empty()) {
        error = "Student email is required.";
        return false;
    }
    if (clean.universitySlug.empty() || clean.universitySlug.size() > 160) {
        error = "Choose a valid host university.";
        return false;
    }
    if (input.courses.size() > MAX_COURSES_PER_PLAN) {
        error = "You can save at most 15 courses per university plan.";
        return false;
    }

    for (int i = 0; i < input.courses.size(); i++) {
        PlannedCourse c;
        c.hostCourseCode = trim(input.courses[i].hostCourseCode);
        c.hostCourseTitle = trim(input.courses[i].hostCourseTitle);
        c.aucEquivalent = trim(input.courses[i].aucEquivalent);
        c.effectiveTerm = trim(input.courses[i].effectiveTerm);
        c.source = trim(input.courses[i].source);
        if (c.source != "banner") c.source = "manual";

        // Skip completely empty rows
        if (c.hostCourseCode.empty() && c.hostCourseTitle.empty() && c.aucEquivalent.empty()) {
            continue;
        }
        clean.courses.push_back(c);
    }

    if (!repo.upsertPlan(clean)) {
        error = "Could not save your course plan to the database.";
        return false;
    }

    string key = makeKey(clean.studentEmail, clean.universitySlug);
    bool existed = plansByKey.search(key);
    plansByKey.insert(key, clean);

    if (!existed) {
        Vector<string>* list = slugsByStudent.get(clean.studentEmail);
        if (!list) {
            Vector<string> fresh;
            fresh.push_back(clean.universitySlug);
            slugsByStudent.insert(clean.studentEmail, fresh);
        } else {
            list->push_back(clean.universitySlug);
        }
    }

    return true;
}

bool CoursePlanService::deletePlan(const string& email, const string& universitySlug, string& error) {
    string normEmail = lower(trim(email));
    string slug = trim(universitySlug);
    if (normEmail.empty() || slug.empty()) {
        error = "University slug is required.";
        return false;
    }

    if (!repo.deletePlan(normEmail, slug)) {
        error = "Could not delete the course plan.";
        return false;
    }

    string key = makeKey(normEmail, slug);
    plansByKey.remove(key);

    Vector<string>* list = slugsByStudent.get(normEmail);
    if (list) {
        Vector<string> kept;
        for (int i = 0; i < list->size(); i++) {
            if ((*list)[i] != slug) kept.push_back((*list)[i]);
        }
        slugsByStudent.insert(normEmail, kept);
    }

    return true;
}
