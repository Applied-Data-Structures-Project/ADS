#include <iostream>
#include "include/heap.hpp"

// example record: a possible match with a compatibility score
struct Match {
    int studentId;
    int score;
};

// higher score = higher priority
struct BestScoreFirst {
    bool operator()(const Match& first, const Match& second) const {
        return first.score > second.score;
    }
};

int main() {
    int values[] = {5, 1, 9, 3, 7};
    int topValue;

    // max-heap of ints (default comparator)
    Heap<int> maxHeap;
    for (int index = 0; index < 5; index++) {
        maxHeap.push(values[index]);
    }
    while (maxHeap.pop(topValue)) {
        std::cout << topValue << " ";  // expected: 9 7 5 3 1
    }
    std::cout << "\n";

    // min-heap of ints built from an array
    Heap<int, MinCompare<int> > minHeap;
    minHeap.buildFromArray(values, 5);
    while (minHeap.pop(topValue)) {
        std::cout << topValue << " ";  // expected: 1 3 5 7 9
    }
    std::cout << "\n";

    // heap of matches, best score on top
    Heap<Match, BestScoreFirst> matchHeap;
    Match matches[] = { {101, 70}, {102, 95}, {103, 60}, {104, 88} };
    for (int index = 0; index < 4; index++) {
        matchHeap.push(matches[index]);
    }

    // student 103's score was recalculated: search the heap for them, then update
    Match currentMatch;
    for (int index = 0; index < matchHeap.size(); index++) {
        matchHeap.getAt(index, currentMatch);
        if (currentMatch.studentId == 103) {
            Match updatedMatch = {103, 99};
            matchHeap.updateAt(index, updatedMatch);
            break;
        }
    }

    // print the top 3 matches
    Match bestMatch;
    for (int rank = 1; rank <= 3; rank++) {
        if (matchHeap.pop(bestMatch)) {
            std::cout << "#" << rank << " student " << bestMatch.studentId
                      << " score " << bestMatch.score << "\n";
        }
    }
    // expected: #1 student 103 score 99, #2 student 102 score 95, #3 student 104 score 88
    return 0;
}
