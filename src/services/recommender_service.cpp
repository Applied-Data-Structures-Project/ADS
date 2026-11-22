// recommender_service.cpp
// Weighted scoring + O(n + k log n) top-K extraction using our custom Heap (include/heap.hpp).
// No STL containers used.

#include "services/recommender_service.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "mergesort.hpp"
#include "services/student_service.hpp"

using namespace std;

namespace {

string toLower(const string& text) {
    string out = text;
    for (char& c : out) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return out;
}

bool containsCI(const string& haystack, const string& needle) {
    if (needle.empty()) return false;
    return toLower(haystack).find(toLower(needle)) != string::npos;
}

// Parse a simple JSON string array such as ["UG", "GR"] into our custom Vector<string>.
Vector<string> parseJsonStringArray(const string& json) {
    Vector<string> out;
    size_t i = 0;
    while (i < json.size()) {
        while (i < json.size() && json[i] != '"') i++;
        if (i >= json.size()) break;
        i++; // skip opening quote
        string item;
        while (i < json.size() && json[i] != '"') {
            if (json[i] == '\\' && i + 1 < json.size()) {
                i++;
                item += json[i];
            } else {
                item += json[i];
            }
            i++;
        }
        if (i < json.size()) i++; // skip closing quote
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

// Deterministic FNV-1a hash for consistent estimates when fields are unknown.
uint32_t fnv1a(const string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

// Deterministic deadline in Dec 2026 .. Feb 2027 (matching the student portal's cycle window).
string estimateDeadlineIso(const string& slug) {
    // 90-day window starting 2026-12-01 (Dec: 31 days, Jan 2027: 31 days, Feb 2027: 28 days)
    int offset = static_cast<int>(fnv1a(slug + "|deadline") % 90u);
    int year = 2026;
    int month = 12;
    int day = 1 + offset;
    if (day > 31) {
        day -= 31;
        year = 2027;
        month = 1;
    }
    if (month == 1 && day > 31) {
        day -= 31;
        month = 2;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    return string(buf);
}

int estimateSemesterCostUsd(const University& u) {
    bool waived = (u.tuitionModel == "auc_exchange_waiver");
    uint32_t h = fnv1a(u.slug);

    int tuitionHalf = 0;
    if (!waived) {
        int annual = 6000 + static_cast<int>(h % 180) * 100;
        tuitionHalf = annual / 2;
    }

    int monthlyLiving = 0;
    if (u.livingCostUsdEst > 0.0) {
        monthlyLiving = static_cast<int>(round(u.livingCostUsdEst));
        if (monthlyLiving < 300) monthlyLiving = 300;
        if (monthlyLiving > 3500) monthlyLiving = 3500;
    } else {
        // Regional baseline + small deterministic slug offset.
        string reg = toLower(u.region);
        int base = 850;
        if (reg.find("north america") != string::npos) base = 1150;
        else if (reg.find("europe") != string::npos) base = 950;
        else if (reg.find("asia") != string::npos) base = 750;
        else if (reg.find("middle east") != string::npos || reg.find("africa") != string::npos) base = 650;
        monthlyLiving = base + static_cast<int>((h >> 8) % 300);
    }

    int hostFees = 250 + static_cast<int>((h >> 16) % 600);
    int total = tuitionHalf + monthlyLiving * 5 + hostFees;
    return ((total + 5) / 10) * 10;
}

// Broad discipline bucket matching so specialized agreements (e.g. Business, Economics,
// Political Science, Journalism, Humanities, Law) match official AUC majors cleanly.
bool matchesBucket(const string& text, const char* const* keywords, int count) {
    string low = toLower(text);
    for (int i = 0; i < count; i++) {
        if (low.find(keywords[i]) != string::npos) return true;
    }
    return false;
}

bool majorMatchesList(const string& studentMajor, const Vector<string>& eligibleList) {
    if (studentMajor.empty()) return false;

    static const char* kBusiness[] = {
        "business", "accounting", "finance", "marketing", "management",
        "mba", "emba", "cems", "mict", "b.a.c", "b.b.e", "b.b.f", "b.b.m", "entrepreneur"
    };
    static const char* kEcon[] = {"econ"};
    static const char* kPoli[] = {
        "political", "pols", "global affairs", "public policy", "public administration",
        "migration", "refugee", "cmeps", "mga", "mpa", "mpp", "human rights"
    };
    static const char* kMedia[] = {
        "journalism", "media", "communication", "film", "television", "jrmc", "imc"
    };
    static const char* kHumanities[] = {
        "humanities", "history", "philosophy", "english", "literature", "arabic",
        "egyptology", "coptology", "anthropology", "sociology", "psychology",
        "middle east", "gender", "theatre", "music", "visual arts", "graphic design",
        "film", "education", "tafl", "tesol"
    };
    static const char* kLaw[] = {"law", "ll.m", "legal"};
    static const char* kEngineering[] = {
        "engineering", "computer", "computing", "data science", "architecture",
        "robotics", "nanotechnology", "petroleum", "mechanical", "electronics",
        "construction", "environmental"
    };

    for (int i = 0; i < eligibleList.size(); i++) {
        string entry = toLower(eligibleList[i]);
        if (entry == "all") return true;

        // Direct substring check
        if (entry.size() >= 3 && (containsCI(studentMajor, entry) || containsCI(entry, studentMajor))) {
            return true;
        }
        // Bucket checks
        if (matchesBucket(entry, kBusiness, 14) && matchesBucket(studentMajor, kBusiness, 14)) return true;
        if (matchesBucket(entry, kEcon, 1) && matchesBucket(studentMajor, kEcon, 1)) return true;
        if (matchesBucket(entry, kPoli, 12) && matchesBucket(studentMajor, kPoli, 12)) return true;
        if (matchesBucket(entry, kMedia, 7) && matchesBucket(studentMajor, kMedia, 7)) return true;
        if (matchesBucket(entry, kHumanities, 20) && matchesBucket(studentMajor, kHumanities, 20)) return true;
        if (matchesBucket(entry, kLaw, 3) && matchesBucket(studentMajor, kLaw, 3)) return true;
        if (matchesBucket(entry, kEngineering, 12) && matchesBucket(studentMajor, kEngineering, 12)) return true;
    }
    return false;
}

bool isMajorExcluded(const string& studentMajor, const string& studentLevel,
                     const Vector<string>& excludedList, string& matchedEntry) {
    for (int i = 0; i < excludedList.size(); i++) {
        string low = toLower(excludedList[i]);
        if (low.find("graduate") != string::npos && low.find("except") == string::npos) {
            if (studentLevel == "GR") {
                matchedEntry = excludedList[i];
                return true;
            }
            continue;
        }
        if (low.find("course") != string::npos &&
            low.find("medicine") == string::npos &&
            low.find("law") == string::npos) {
            continue;
        }
        if (containsCI(studentMajor, "law") && low.find("law") != string::npos) {
            matchedEntry = excludedList[i];
            return true;
        }
    }
    return false;
}

string formatGpa(double gpa) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", gpa);
    return string(buf);
}

// Wrapper placed inside Heap<ScoredCandidate> so MaxCompare uses operator>.
struct ScoredCandidate {
    UniversityRecommendation rec;

    bool operator>(const ScoredCandidate& other) const {
        // 1. Eligible universities always rank above ineligible ones.
        if (rec.eligible != other.rec.eligible) return rec.eligible && !other.rec.eligible;
        // 2. Higher fit score wins.
        if (rec.fitScore != other.rec.fitScore) return rec.fitScore > other.rec.fitScore;
        // 3. Lower estimated semester cost breaks ties.
        if (rec.estimatedSemesterCostUsd != other.rec.estimatedSemesterCostUsd) {
            return rec.estimatedSemesterCostUsd < other.rec.estimatedSemesterCostUsd;
        }
        // 4. Deterministic alphabetical tie-breaker by slug.
        return rec.slug < other.rec.slug;
    }
};

// Wrapper for Heap<DeadlineItem, MinCompare<DeadlineItem>> (nearest deadline first).
struct DeadlineCandidate {
    UniversityRecommendation rec;

    bool operator<(const DeadlineCandidate& other) const {
        if (rec.deadline != other.rec.deadline) return rec.deadline < other.rec.deadline;
        if (rec.fitScore != other.rec.fitScore) return rec.fitScore > other.rec.fitScore;
        return rec.slug < other.rec.slug;
    }
};

} // namespace

UniversityRecommendation RecommenderService::scoreUniversity(const StudentRecord& student,
                                                             const University& u) {
    UniversityRecommendation rec;
    rec.slug = u.slug;
    rec.name = u.name;
    rec.country = u.country;
    rec.city = u.city;
    rec.region = u.region;
    rec.minGpaHost = u.minGpa;
    rec.estimatedSemesterCostUsd = estimateSemesterCostUsd(u);
    rec.withinBudget = (student.hasBudget && student.budget > 0 &&
                        rec.estimatedSemesterCostUsd <= student.budget);
    rec.deadline = estimateDeadlineIso(u.slug);

    Vector<string> levels = parseJsonStringArray(u.studentLevelsJson);
    Vector<string> eligibleMajors = parseJsonStringArray(u.eligibleMajorsJson);
    Vector<string> excludedMajors = parseJsonStringArray(u.excludedMajorsJson);

    bool hasProfile = student.eligibilitySaved && (student.level == "UG" || student.level == "GR");
    StudentEligibilityOutcome ipso = StudentService::evaluateEligibility(student);
    bool isPhd = containsCI(student.major, "ph.d") || containsCI(student.major, "phd");

    // ---- Eligibility evaluation ----
    bool levelOk = true;
    if (hasProfile && levels.size() > 0) {
        levelOk = false;
        for (int i = 0; i < levels.size(); i++) {
            if (levels[i] == student.level || (isPhd && levels[i] == "PhD")) {
                levelOk = true;
                break;
            }
        }
    }

    bool provisionalGr = (hasProfile && student.level == "GR" &&
                          student.firstSemesterNoGpa && !student.hasGpa);
    bool hostGpaOk = true;
    if (hasProfile && u.minGpa > 0.0 && !provisionalGr) {
        hostGpaOk = (student.hasGpa && student.gpa >= u.minGpa);
    }

    bool openToAllMajors = (eligibleMajors.size() == 0);
    for (int i = 0; i < eligibleMajors.size(); i++) {
        if (toLower(eligibleMajors[i]) == "all") {
            openToAllMajors = true;
            break;
        }
    }
    bool specializedMajorMatch = (!openToAllMajors && hasProfile &&
                                  majorMatchesList(student.major, eligibleMajors));
    bool majorOk = openToAllMajors || !hasProfile || specializedMajorMatch;

    string excludedEntry;
    bool excluded = hasProfile && isMajorExcluded(student.major, student.level, excludedMajors, excludedEntry);

    if (!hasProfile) {
        rec.eligible = false;
        rec.eligibilityReason = "Complete your eligibility profile first.";
    } else if (!ipso.eligible) {
        rec.eligible = false;
        rec.eligibilityReason = StudentService::checkEligibilityReason(student);
    } else if (!levelOk) {
        rec.eligible = false;
        rec.eligibilityReason = "Does not accept " + student.level + " students.";
    } else if (!hostGpaOk) {
        rec.eligible = false;
        rec.eligibilityReason = "Host requires " + formatGpa(u.minGpa) + " GPA.";
    } else if (!majorOk) {
        rec.eligible = false;
        rec.eligibilityReason = "Restricted to specific majors.";
    } else if (excluded) {
        rec.eligible = false;
        rec.eligibilityReason = "Excluded under host agreement (" + excludedEntry + ").";
    } else {
        rec.eligible = true;
        rec.provisional = ipso.provisional;
        rec.eligibilityReason = "";
    }

    // ---- Weighted fit score (0..100) ----
    int score = 0;

    // 1. Level + agreement status (max 25)
    if (levelOk) score += 15;
    if (u.agreementStatus == "active") score += 10;
    else if (u.agreementStatus == "check_slots") score += 5;
    else score += 2;

    // 2. Major alignment (max 20)
    if (specializedMajorMatch) score += 20;
    else if (openToAllMajors) score += 14;

    // 3. GPA strength (max 25)
    double floorGpa = (u.minGpa > 0.0) ? u.minGpa : ((student.level == "GR") ? 3.2 : 2.8);
    if (provisionalGr) {
        score += 15;
    } else if (student.hasGpa && student.gpa > 0.0) {
        double margin = student.gpa - floorGpa;
        if (margin >= 0.0) {
            int gpaPts = 14 + static_cast<int>(round(margin * 14.0));
            if (gpaPts > 25) gpaPts = 25;
            score += gpaPts;
        } else {
            int penalty = static_cast<int>(round(margin * 20.0));
            int gpaPts = 10 + penalty;
            if (gpaPts < 0) gpaPts = 0;
            score += gpaPts;
        }
    } else {
        score += 10;
    }

    // 4. Budget & tuition waiver fit (max 20)
    bool waived = (u.tuitionModel == "auc_exchange_waiver");
    if (student.hasBudget && student.budget > 0) {
        if (rec.estimatedSemesterCostUsd <= student.budget) {
            score += 20;
        } else {
            double ratio = static_cast<double>(student.budget) /
                           static_cast<double>(rec.estimatedSemesterCostUsd);
            int budgetPts = static_cast<int>(round(ratio * 14.0));
            if (budgetPts < 0) budgetPts = 0;
            if (budgetPts > 14) budgetPts = 14;
            score += budgetPts;
        }
    } else {
        score += waived ? 14 : 6;
    }

    // 5. English / writing readiness (max 10)
    if (student.englishComplete || student.rhet120) score += 10;

    if (score < 0) score = 0;
    if (score > 100) score = 100;
    rec.fitScore = score;

    // Concise human-readable match summary
    if (!rec.eligible && !rec.eligibilityReason.empty()) {
        rec.matchSummary = rec.eligibilityReason;
    } else if (specializedMajorMatch && rec.withinBudget) {
        rec.matchSummary = "Direct major match and within your budget";
    } else if (specializedMajorMatch) {
        rec.matchSummary = "Specialized agreement for your major";
    } else if (rec.withinBudget && waived) {
        rec.matchSummary = "Exchange tuition waiver and within your budget";
    } else if (rec.withinBudget) {
        rec.matchSummary = "Fits your academic profile and semester budget";
    } else if (waived) {
        rec.matchSummary = "Active AUC exchange partner with tuition waiver";
    } else {
        rec.matchSummary = "Matches your academic profile";
    }

    return rec;
}

Vector<UniversityRecommendation> RecommenderService::recommend(
    const StudentRecord& student,
    const Vector<University>& universities,
    const DiscoveryService* discovery,
    int k) {

    Vector<UniversityRecommendation> result;
    if (k <= 0 || universities.size() == 0) return result;

    // 1. Score all unsaved universities into a flat array for O(n) heapify.
    Vector<ScoredCandidate> candidates;
    candidates.reserve(universities.size());

    for (int i = 0; i < universities.size(); i++) {
        const University& u = universities[i];
        if (discovery != nullptr && !student.email.empty() &&
            discovery->isSaved(student.email, u.slug)) {
            continue;   // O(1) HashMap lookup skips already-saved schools
        }
        ScoredCandidate c;
        c.rec = scoreUniversity(student, u);
        candidates.push_back(c);
    }

    if (candidates.size() == 0) return result;

    // 2. Build our custom Max-Heap in O(n) using Floyd's buildFromArray.
    Heap<ScoredCandidate> maxHeap;
    maxHeap.buildFromArray(candidates.begin(), candidates.size());

    // 3. Pop the top K recommendations in O(k log n).
    int limit = k < maxHeap.size() ? k : maxHeap.size();
    result.reserve(limit);
    for (int i = 0; i < limit; i++) {
        ScoredCandidate best;
        if (!maxHeap.pop(best)) break;
        result.push_back(best.rec);
    }

    return result;
}

Vector<UniversityRecommendation> RecommenderService::sortSaved(
    const StudentRecord& student,
    const Vector<University>& savedUniversities,
    const string& sortBy) {

    Vector<UniversityRecommendation> scored;
    scored.reserve(savedUniversities.size());
    for (int i = 0; i < savedUniversities.size(); i++) {
        scored.push_back(scoreUniversity(student, savedUniversities[i]));
    }

    if (scored.size() <= 1) return scored;

    if (sortBy == "deadline") {
        // Nearest deadline first using our C++ Min-Heap (include/heap.hpp).
        Vector<DeadlineCandidate> items;
        items.reserve(scored.size());
        for (int i = 0; i < scored.size(); i++) {
            DeadlineCandidate dc;
            dc.rec = scored[i];
            items.push_back(dc);
        }
        Heap<DeadlineCandidate, MinCompare<DeadlineCandidate> > minHeap;
        minHeap.buildFromArray(items.begin(), items.size());

        Vector<UniversityRecommendation> ordered;
        ordered.reserve(items.size());
        DeadlineCandidate next;
        while (minHeap.pop(next)) {
            ordered.push_back(next.rec);
        }
        return ordered;
    }

    if (sortBy == "fit") {
        // Best fit first using our stable C++ mergeSort (include/mergesort.hpp).
        mergeSort(scored, [](const UniversityRecommendation& a, const UniversityRecommendation& b) {
            if (a.fitScore != b.fitScore) return a.fitScore > b.fitScore;
            if (a.estimatedSemesterCostUsd != b.estimatedSemesterCostUsd)
                return a.estimatedSemesterCostUsd < b.estimatedSemesterCostUsd;
            return a.slug < b.slug;
        });
        return scored;
    }

    if (sortBy == "cost") {
        // Lowest semester cost first using our stable C++ mergeSort (include/mergesort.hpp).
        mergeSort(scored, [](const UniversityRecommendation& a, const UniversityRecommendation& b) {
            if (a.estimatedSemesterCostUsd != b.estimatedSemesterCostUsd)
                return a.estimatedSemesterCostUsd < b.estimatedSemesterCostUsd;
            if (a.fitScore != b.fitScore) return a.fitScore > b.fitScore;
            return a.slug < b.slug;
        });
        return scored;
    }

    return scored;
}
