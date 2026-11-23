#pragma once
#include <iostream>
#include <algorithm>
#include <limits>
#include "vector.hpp"
#include "hashmap.hpp" //for now until we implement our own hashmap
#include "queue.hpp"

// implemented our own pair struct so we can avoid stl
template<class U, class V>
struct Pair {
    U first;
    V second;
    Pair() : first(), second() {}
    Pair(const U& f, const V& s) : first(f), second(s) {}
};

struct Edge {
    int to;
    double weight;
};

template<class T1,class T2>
class BipartiteGraph{
    public:
        BipartiteGraph();
        void add_edge(T1 set1_item, T2 set2_item, double weight = 1.0);

        Vector<Pair<T1, T2>> get_max_cardinality_matching(); //returns result of Hopcroft-Karp
        Vector<Pair<T1,T2>> get_max_weight_matching(); //returns result of Hungarian algorithm
        Vector<Pair<T1,T2>> get_max_weight_matching_jv(); //returns result of Jonker-Volgenant algorithm
        void debug_print(); // Testing the graph (Function made by AI)

    private:
        void add_node_set1(T1 item);
        void add_node_set2(T2 item);

        int num_set1; // Number of students for example
        int num_set2; // Number of internships for example
        enum { HK_INF = 1000000000 }; // "infinite" distance for Hopcroft-Karp layers
        HashMap<T1, int> set1_ids; // Name to ID
        HashMap<T2, int> set2_ids;
        Vector<T1> id_to_set1; // ID to Name (reverse map)
        Vector<T2> id_to_set2;

        Vector<Vector<Edge>> adj_list; //for Hopcroft-Karp
        Vector<int> match_set1,match_set2,dist;

        Vector<Vector<double>> cost_matrix; //for Hungarian algorithm

        bool hk_bfs();
        bool hk_dfs(int set1_id);

        void prepare_hungarian_matrix();
        void solve_hungarian();
        void solve_jonker_volgenant();
        
};

#include "bipartite.tpp"   // function bodies
