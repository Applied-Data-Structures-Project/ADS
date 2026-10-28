#include <iostream>
#include <string>
#include "bipartite.hpp"

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

    return 0;
}
