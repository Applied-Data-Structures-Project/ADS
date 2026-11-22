#pragma once

// recommender_service.hpp
// Top-K partner university recommender powered by our generic Max-Heap (include/heap.hpp).
//
// Algorithm:
//   1. Filter out universities already saved by the student in O(1) via DiscoveryService's HashMap.
//   2. Score each candidate university against the student's profile (level, GPA, major, budget,
//      English readiness, agreement status).
//   3. Build a Max-Heap in O(n) via Heap::buildFromArray (Floyd's algorithm).
//   4. Pop the top K recommendations in O(k log n), with deterministic tie-breaking.

#include <string>
#include "heap.hpp"
#include "vector.hpp"
#include "models/recommendation.hpp"
#include "models/student_profile.hpp"
#include "models/university.hpp"
#include "services/discovery_service.hpp"

class RecommenderService {
public:
    // Score one university for a given student (useful for unit tests and inspection).
    static UniversityRecommendation scoreUniversity(const StudentRecord& student,
                                                    const University& university);

    // Return the top K unsaved universities for this student using Heap<T> (Max-Heap).
    static Vector<UniversityRecommendation> recommend(
        const StudentRecord& student,
        const Vector<University>& universities,
        const DiscoveryService* discovery,
        int k = 3);

    // Score and sort a student's saved universities using our C++ data structures:
    //   "deadline" -> Min-Heap (Heap<T, MinCompare<T>>)
    //   "fit"      -> Stable mergeSort (include/mergesort.hpp), highest fit first
    //   "cost"     -> Stable mergeSort (include/mergesort.hpp), lowest cost first
    static Vector<UniversityRecommendation> sortSaved(
        const StudentRecord& student,
        const Vector<University>& savedUniversities,
        const std::string& sortBy);
};
