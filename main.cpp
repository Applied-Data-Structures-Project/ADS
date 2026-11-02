#include <iostream>
#include <string>
#include "bipartite.hpp"
#include "hashmap.hpp"
#include "heap.hpp"

// these are just so we can display the data
// clearer for the heap section
struct Match {
    int studentId;
    int score;
};

struct BestScoreFirst {
    bool operator()(const Match& first, const Match& second) const {
        return first.score > second.score;
    }
};

int main() {
    BipartiteGraph<std::string,std::string> graph;
 //Testing the graph
 
    graph.add_edge("Alice", "Google", 0.95);
    graph.add_edge("Alice", "Meta", 0.70);

    graph.add_edge("Bob", "Google", 0.80);
    graph.add_edge("Bob", "Microsoft", 0.90);

    graph.add_edge("Charlie", "Meta", 0.85);
    graph.add_edge("Charlie", "Apple", 0.60);

    graph.add_edge("David", "Microsoft", 0.75);
    graph.add_edge("David", "Apple", 0.95);

    std::cout << "\nPhase 1: Verification after edge insertion\n";
    graph.debug_print();


    /// ================= Testing the HashMap =================
    cout << "\nPhase 2: HashMap verification\n";
    cout << "=== HASHMAP DEBUG INFO ===\n";

    // --- insert ---
    HashMap<string,int> ids;   // student name -> student ID
    ids.insert("Alice", 0);
    ids.insert("Bob", 1);
    ids.insert("Charlie", 2);
    cout << "\n--- INSERT ---\n";
    cout << "  inserted Alice, Bob, Charlie\n";
    cout << "  size: " << ids.size() << "\n";

    // --- search ---
    cout << "\n--- SEARCH ---\n";
    cout << "  Bob:    " << (ids.search("Bob") ? "found" : "not found") << "\n";
    cout << "  Nobody: " << (ids.search("Nobody") ? "found" : "not found") << "\n";

    // --- get ---
    cout << "\n--- GET ---\n";
    int* charlieId = ids.get("Charlie");
    if (charlieId != nullptr) {
        cout << "  Charlie's ID: " << *charlieId << "\n";
    }
    cout << "  Nobody: " << (ids.get("Nobody") == nullptr ? "nullptr" : "has a value") << "\n";

    // --- duplicate key ---
    cout << "\n--- DUPLICATE KEY ---\n";
    ids.insert("Bob", 10);
    cout << "  inserted Bob again with ID 10\n";
    cout << "  Bob's ID: " << *ids.get("Bob") << "\n";
    cout << "  size: " << ids.size() << "\n";

    // --- remove ---
    cout << "\n--- REMOVE ---\n";
    cout << "  remove Alice:       " << (ids.remove("Alice") ? "removed" : "not found") << "\n";
    cout << "  remove Alice again: " << (ids.remove("Alice") ? "removed" : "not found") << "\n";
    cout << "  size: " << ids.size() << "\n";

    // --- table contents ---
    cout << "\n--- TABLE (boxes and chains) ---\n";
    ids.printTable();

    // --- forEach ---
    cout << "\n--- FOREACH (visit every pair) ---\n";
    int visits = 0;
    ids.forEach([&](const string& name, const int& value) {
        cout << "  " << name << " -> " << value << "\n";
        visits++;
    });
    cout << "  visits: " << visits << ", size: " << ids.size() << "\n";

    
    cout << "\n--- REHASH (start with 2 boxes, insert 20 keys) ---\n";
    HashMap<string,int> big(2);
    for (int i = 0; i < 20; i++) {
        big.insert("student" + to_string(i), i);
    }
    cout << "  size: " << big.size() << "\n";
    cout << "  load factor: " << big.loadFactor() << "\n";
    cout << "  student15's ID: " << *big.get("student15") << "\n";

    int sum = 0;
    big.forEach([&](const string& name, const int& value) {
        sum += value;
    });
    cout << "  sum of all IDs: " << sum << "\n";

    cout << "==========================\n";


    //Testing the Hopcroft-Karp algorithm
    std::cout << "\nPhase 3: Hopcroft-Karp algorithm\n";

    Vector<Pair<std::string, std::string>> m = graph.get_max_cardinality_matching();

    std::cout << "Matching size: " << m.size()<< "\n";
    for (int i = 0; i < m.size(); ++i) {
        std::cout << m[i].first << " <-> " << m[i].second << "\n";
    }

    // test hungarian
    std::cout << "\nPhase 4: Hungarian Algorithm (Max Weight Matching)\n";
    std::cout << "(Finds optimal pairing using compatibility weights rather than just max count)\n";
    
    Vector<Pair<std::string, std::string>> weighted_m = graph.get_max_weight_matching();

    std::cout << "Weighted Matching size: " << weighted_m.size() << "\n";
    for (int i = 0; i < weighted_m.size(); ++i) {
        std::cout << "  " << weighted_m[i].first << " <-> " << weighted_m[i].second << "\n";
    }

    // test heap
    std::cout << "\nPhase 5: Binary Heap (Priority Queue for Waitlists/Ranking)\n";
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
