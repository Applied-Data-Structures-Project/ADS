#include <iostream>
#include <string>
#include "bipartite.hpp"
#include "hashmap.hpp"

int main() {
    BipartiteGraph<std::string> graph;
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
    HashMap<int> ids;   // student name -> student ID
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
    HashMap<int> big(2);
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

    return 0;
}
