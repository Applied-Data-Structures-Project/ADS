#pragma once
#include "vector.hpp"
#include <unordered_map> //for now until we implement our own hashmap

struct Edge {
    int to;
    double weight;
};

template<class T>
class BipartiteGraph{
    public:
        BipartiteGraph();
        void add_node(T item, bool is_set1);
        void add_edge(T set1, T set2, double weight = 1.0);

        Vector<std::pair<T, T>> get_max_cardinality_matching(); //returns result of Hopcroft-Karp
        Vector<std::pair<T, T>> get_max_weight_matching(); //returns result of Hungarian algorithm
        void debug_print(); // Testing the graph (Function made by AI)

    private:
        int num_set1; // Number of students for example
        int num_set2; // Number of internships for example
        std::unordered_map<T, int> set1_ids; // Name to ID
        std::unordered_map<T, int> set2_ids;
        Vector<T> id_to_set1; // ID to Name (reverse map)
        Vector<T> id_to_set2;

        Vector<Vector<Edge>> adj_list; //for Hopcroft-Karp
        Vector<int> match_set1,match_set2,dist;

        Vector<Vector<double>> cost_matrix; //for Hungarian algorithm

        bool hk_bfs();
        bool hk_dfs(int set1_id);

        void prepare_hungarian_matrix();
        void solve_hungarian();
        
};

template<class T>
BipartiteGraph<T>::BipartiteGraph(): num_set1(0), num_set2(0){}

template<class T>
void BipartiteGraph<T>::add_node(T item, bool is_set1){
    if(is_set1){
        set1_ids[item] = num_set1;
        if (num_set1 >= id_to_set1.size()) {
            id_to_set1.resize(num_set1 + 1);
        }
        id_to_set1[num_set1++] = item;
        adj_list.resize(num_set1);
    }else{
        set2_ids[item] = num_set2;
        if (num_set2 >= id_to_set2.size()) {
            id_to_set2.resize(num_set2 + 1);
        }
        id_to_set2[num_set2++] = item;
    }
}

template<class T>
void BipartiteGraph<T>::add_edge(T set1, T set2, double weight){
    int set1_id, set2_id;
    try {
        set1_id = set1_ids.at(set1);
    } catch (const std::out_of_range& e) {
        add_node(set1, true);
        set1_id = set1_ids.at(set1);
    }
    try {
        set2_id = set2_ids.at(set2);
    } catch (const std::out_of_range& e) {
        add_node(set2, false);
        set2_id = set2_ids.at(set2);
    }

    adj_list[set1_id].push_back({set2_id, weight});
}

template<class T>
bool BipartiteGraph<T>::hk_bfs(){
    //TODO
}

template<class T>
bool BipartiteGraph<T>::hk_dfs(int set1_id){
    //TODO
}

template<class T>
void BipartiteGraph<T>::prepare_hungarian_matrix() {
    int K = std::max(num_set1, num_set2);

    cost_matrix.assign(K, Vector<double>(K, 0.0));

    for (int i = 0; i < num_set1; ++i) {
        for (const Edge& edge : adj_list[i]) {
            cost_matrix[i][edge.to] = edge.weight;
        }
    }

    //Uncomplete only resizes the array for now
}

template<class T>
void BipartiteGraph<T>::solve_hungarian() {
    //TODO
}
template<class T>
Vector<std::pair<T, T>> BipartiteGraph<T>::get_max_cardinality_matching() {
    //TODO
}

template<class T>
Vector<std::pair<T, T>> BipartiteGraph<T>::get_max_weight_matching() {
    //TODO
}

template<class T>
void BipartiteGraph<T>::debug_print() {
    std::cout << "=== GRAPH DEBUG INFO ===\n\n";

    std::cout << "--- SET 1 (Students) [" << num_set1 << "] ---\n";
    for (int i = 0; i < num_set1; ++i) {
        std::cout << "  ID " << i << ": " << id_to_set1[i] << "\n";
    }

    std::cout << "\n--- SET 2 (Internships) [" << num_set2 << "] ---\n";
    for (int i = 0; i < num_set2; ++i) {
        std::cout << "  ID " << i << ": " << id_to_set2[i] << "\n";
    }

    std::cout << "\n--- ADJACENCY LIST (Unweighted / HK) ---\n";
    for (int i = 0; i < num_set1; ++i) {
        std::cout << "  " << id_to_set1[i] << " -> ";
        for (const auto& edge : adj_list[i]) {
            std::cout << id_to_set2[edge.to] << " (weight: " << edge.weight << ") ";
        }
        std::cout << "\n";
    }

    std::cout << "========================\n";
}

