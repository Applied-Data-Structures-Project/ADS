#pragma once
#include <iostream>
#include <algorithm>
#include <limits>
#include "vector.hpp"
#include "hashmap.hpp" //for now until we implement our own hashmap

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

template<class T>
class BipartiteGraph{
    public:
        BipartiteGraph();
        void add_node(T item, bool is_set1);
        void add_edge(T set1, T set2, double weight = 1.0);

        Vector<Pair<T, T>> get_max_cardinality_matching(); //returns result of Hopcroft-Karp
        Vector<Pair<T, T>> get_max_weight_matching(); //returns result of Hungarian algorithm
        Vector<Pair<T, T>> get_max_weight_matching_jv(); //returns result of Jonker-Volgenant algorithm
        void debug_print(); // Testing the graph (Function made by AI)

    private:
        int num_set1; // Number of students for example
        int num_set2; // Number of internships for example
        enum { HK_INF = 1000000000 }; // "infinite" distance for Hopcroft-Karp layers
        HashMap<T, int> set1_ids; // Name to ID
        HashMap<T, int> set2_ids;
        Vector<T> id_to_set1; // ID to Name (reverse map)
        Vector<T> id_to_set2;

        Vector<Vector<Edge>> adj_list; //for Hopcroft-Karp
        Vector<int> match_set1,match_set2,dist;

        Vector<Vector<double>> cost_matrix; //for Hungarian algorithm

        bool hk_bfs();
        bool hk_dfs(int set1_id);

        void prepare_hungarian_matrix();
        void solve_hungarian();
        void solve_jonker_volgenant();
        
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
    dist.assign(num_set1, HK_INF);

    // Queue implemented with a Vector and a head index (no std::queue)
    Vector<int> queue;
    int head = 0;

    // Every free set1 node starts at layer 0
    for (int u = 0; u < num_set1; ++u) {
        if (match_set1[u] == -1) {
            dist[u] = 0;
            queue.push_back(u);
        }
    }

    int shortest = HK_INF; // layer of the first free set2 node we reach
    bool found = false;

    while (head < (int)queue.size()) {
        int u = queue[head++];

        // Nodes deeper than the shortest augmenting path are useless
        if (dist[u] >= shortest) continue;

        for (int k = 0; k < (int)adj_list[u].size(); ++k) {
            int v = adj_list[u][k].to;
            int w = match_set2[v]; // set1 node currently matched to v

            if (w == -1) {
                // v is free -> augmenting path ends here
                found = true;
                shortest = dist[u] + 1;
            } else if (dist[w] == HK_INF) {
                // first time reaching w: place it in the next layer
                dist[w] = dist[u] + 1;
                queue.push_back(w);
            }
        }
    }
    return found;
}

template<class T>
bool BipartiteGraph<T>::hk_dfs(int set1_id){
    for (int k = 0; k < (int)adj_list[set1_id].size(); ++k) {
        int v = adj_list[set1_id][k].to;
        int w = match_set2[v];

        // Case 1: v is free -> path ends here.
        // Case 2: v is matched to w in the next layer and w can be re-routed.
        if (w == -1 || (dist[w] == dist[set1_id] + 1 && hk_dfs(w))) {
            match_set1[set1_id] = v;
            match_set2[v] = set1_id;
            return true;
        }
    }

    // Dead end: remove this node from the layered graph for this phase
    dist[set1_id] = HK_INF;
    return false;
}

template<class T>
Vector<Pair<T, T>> BipartiteGraph<T>::get_max_cardinality_matching() {
    Vector<Pair<T, T>> result;
    if (num_set1 == 0 || num_set2 == 0) return result;

    // Start with an empty matching (-1 = unmatched)
    match_set1.assign(num_set1, -1);
    match_set2.assign(num_set2, -1);

    // Each successful BFS means at least one more augmentation is possible
    while (hk_bfs()) {
        for (int u = 0; u < num_set1; ++u) {
            if (match_set1[u] == -1) {
                hk_dfs(u);
            }
        }
    }

    // Convert internal ids back to the user's items
    for (int u = 0; u < num_set1; ++u) {
        if (match_set1[u] != -1) {
            result.push_back({id_to_set1[u], id_to_set2[match_set1[u]]});
        }
    }
    return result;
}

template<class T>
void BipartiteGraph<T>::prepare_hungarian_matrix() {
    // hungarian works on nxn matrix so we make dummy nodes to balance it
    int K = std::max(num_set1, num_set2);

    // hungarian views lower weight as better so we invert cost
    double max_weight = 0.0;
    for (int i = 0; i < num_set1; ++i) {
        for (const Edge& edge : adj_list[i]) {
            if (edge.weight > max_weight) {
                max_weight = edge.weight;
            }
        }
    }

    cost_matrix.assign(K, Vector<double>(K, max_weight));

    for (int i = 0; i < num_set1; ++i) {
        for (const Edge& edge : adj_list[i]) {
            double inverted_cost = max_weight - edge.weight;
            // if more than one edge connects same 2 nodes we only consider lowest cost
            if (inverted_cost < cost_matrix[i][edge.to]) {
                cost_matrix[i][edge.to] = inverted_cost;
            }
        }
    }
}

template<class T>
void BipartiteGraph<T>::solve_hungarian() {
    int K = cost_matrix.size();
    if (K == 0) return;

    const double INF = std::numeric_limits<double>::infinity();

    // 1-indexed potentials (u for rows, v for cols) and matching arrays
    Vector<double> u(K + 1, 0.0);
    Vector<double> v(K + 1, 0.0);
    Vector<int> p(K + 1, 0);   // p[j] = row matched to column j
    Vector<int> way(K + 1, 0); // tracks augmenting path

    for (int i = 1; i <= K; ++i) {
        p[0] = i;
        int j0 = 0; // dummy column 0
        Vector<double> minv(K + 1, INF);
        Vector<bool> used(K + 1, false);

        // Find augmenting path using potentials (Dijkstra-like step)
        do {
            used[j0] = true;
            int i0 = p[j0];
            double delta = INF;
            int j1 = 0;

            for (int j = 1; j <= K; ++j) {
                if (!used[j]) {
                    double cur = cost_matrix[i0 - 1][j - 1] - u[i0] - v[j];
                    if (cur < minv[j]) {
                        minv[j] = cur;
                        way[j] = j0;
                    }
                    if (minv[j] < delta) {
                        delta = minv[j];
                        j1 = j;
                    }
                }
            }

            // Update dual potentials to create new tight edges
            for (int j = 0; j <= K; ++j) {
                if (used[j]) {
                    u[p[j]] += delta;
                    v[j] -= delta;
                } else {
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while (p[j0] != 0);

        // Invert edges along the found augmenting path
        do {
            int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0 != 0);
    }

    // Store final matches (ignoring dummy nodes later)
    match_set1.assign(K, -1);
    match_set2.assign(K, -1);
    for (int j = 1; j <= K; ++j) {
        if (p[j] > 0) {
            match_set1[p[j] - 1] = j - 1;
            match_set2[j - 1] = p[j] - 1;
        }
    }
}

// this is a variant algorithm which is industry standard and gives way better average time complexity than hungarian
// but is a lot longer and more complicated, we can decide whether to keep or not later.
template<class T>
void BipartiteGraph<T>::solve_jonker_volgenant() {
    int K = cost_matrix.size();
    if (K == 0) return;

    const double INF = std::numeric_limits<double>::infinity();
    const double EPS = 1e-9;

    Vector<double> v(K, 0.0);    // Column dual potentials
    match_set1.assign(K, -1);    // Row to Col match
    match_set2.assign(K, -1);    // Col to Row match
    Vector<int> matches(K, 0);   // Count of rows assigned to each col
    Vector<int> free_rows;       // Unassigned rows

    // Phase 1: Column Reduction (find initial min in each column)
    for (int j = K - 1; j >= 0; --j) {
        double min_val = cost_matrix[0][j];
        int i_min = 0;
        for (int i = 1; i < K; ++i) {
            if (cost_matrix[i][j] < min_val) {
                min_val = cost_matrix[i][j];
                i_min = i;
            }
        }
        v[j] = min_val;
        matches[i_min]++;
        if (match_set1[i_min] == -1) {
            match_set1[i_min] = j;
            match_set2[j] = i_min;
        } else if (v[j] < v[match_set1[i_min]]) {
            match_set2[match_set1[i_min]] = -1;
            match_set1[i_min] = j;
            match_set2[j] = i_min;
        }
    }

    // Phase 2: Reduction Transfer (adjust potentials for uniquely matched rows)
    for (int i = 0; i < K; ++i) {
        if (matches[i] == 0) {
            free_rows.push_back(i);
        } else if (matches[i] == 1) {
            int j1 = match_set1[i];
            double min_val = INF;
            for (int j = 0; j < K; ++j) {
                if (j != j1) {
                    double red_cost = cost_matrix[i][j] - v[j];
                    if (red_cost < min_val) {
                        min_val = red_cost;
                    }
                }
            }
            v[j1] -= min_val;
        }
    }

    // Phase 3: Augmenting Row Reduction (two rounds of greedy conflict resolution)
    for (int loop = 0; loop < 2; ++loop) {
        Vector<int> next_free;
        for (int idx = 0; idx < (int)free_rows.size(); ++idx) {
            int i = free_rows[idx];
            double u1 = INF, u2 = INF;
            int j1 = 0, j2 = -1;

            // Find best (j1) and second-best (j2) columns for row i
            for (int j = 0; j < K; ++j) {
                double h = cost_matrix[i][j] - v[j];
                if (h < u2) {
                    if (h >= u1) {
                        u2 = h;
                        j2 = j;
                    } else {
                        u2 = u1;
                        u1 = h;
                        j2 = j1;
                        j1 = j;
                    }
                }
            }

            int i0 = match_set2[j1];
            if (u1 < u2 - EPS) {
                v[j1] = v[j1] - (u2 - u1);
            } else if (i0 != -1 && j2 != -1) {
                j1 = j2;
                i0 = match_set2[j1];
            }

            if (i0 != -1) {
                if (u1 < u2 - EPS) {
                    // Re-evaluate displaced row immediately
                    free_rows[idx] = i0;
                    idx--;
                } else {
                    next_free.push_back(i0);
                }
            }
            match_set1[i] = j1;
            match_set2[j1] = i;
        }
        free_rows = next_free;
    }

    // Phase 4: Augmentation (Shortest Augmenting Path for remaining free rows)
    Vector<double> d(K, INF);
    Vector<int> pred(K, -1);
    Vector<int> col_list(K);

    for (int f = 0; f < (int)free_rows.size(); ++f) {
        int free_row = free_rows[f];
        for (int j = 0; j < K; ++j) {
            d[j] = cost_matrix[free_row][j] - v[j];
            pred[j] = free_row;
            col_list[j] = j;
        }

        int low = 0, up = 0;
        int unassigned_col = -1;
        double min_d = 0.0;

        // Dijkstra search with 3-way partitioned col_list (scanned, tight, remaining)
        while (unassigned_col == -1) {
            if ( up == low ) {
                min_d = d[col_list[up]];
                for (int k = up + 1; k < K; ++k) {
                    int j = col_list[k];
                    if (d[j] <= min_d) {
                        if (d[j] < min_d) {
                            up = low;
                            min_d = d[j];
                        }
                        col_list[k] = col_list[up];
                        col_list[up++] = j;
                    }
                }
                for (int k = low; k < up; ++k) {
                    if (match_set2[col_list[k]] == -1) {
                        unassigned_col = col_list[k];
                        break;
                    }
                }
            }

            if (unassigned_col == -1) {
                int j1 = col_list[low++];
                int i = match_set2[j1];
                double u1 = cost_matrix[i][j1] - v[j1] - min_d;

                for (int k = up; k < K; ++k) {
                    int j = col_list[k];
                    double h = cost_matrix[i][j] - v[j] - u1;
                    if (h < d[j]) {
                        d[j] = h;
                        pred[j] = i;
                        if (std::abs(h - min_d) <= EPS) {
                            if (match_set2[j] == -1) {
                                unassigned_col = j;
                                break;
                            }
                            col_list[k] = col_list[up];
                            col_list[up++] = j;
                        }
                    }
                }
            }
        }

        // Update column potentials for scanned columns
        for (int k = 0; k < low; ++k) {
            int j = col_list[k];
            v[j] += d[j] - min_d;
        }

        // Augment matching along the shortest path
        int j = unassigned_col;
        int i = -1;
        do {
            i = pred[j];
            match_set2[j] = i;
            int next_j = match_set1[i];
            match_set1[i] = j;
            j = next_j;
        } while (i != free_row);
    }
}

template<class T>
Vector<Pair<T, T>> BipartiteGraph<T>::get_max_weight_matching() {
    Vector<Pair<T, T>> result;
    if (num_set1 == 0 || num_set2 == 0) return result;

    prepare_hungarian_matrix();
    solve_hungarian();

    // take the final matching to be only ones where you arent connected to dummy node
    for (int i = 0; i < num_set1; ++i) {
        int matched_j = match_set1[i];
        if (matched_j >= 0 && matched_j < num_set2) {
            // if connection not in original then its dummy so disregard
            bool valid_edge = false;
            for (const Edge& edge : adj_list[i]) {
                if (edge.to == matched_j && edge.weight > 0.0) {
                    valid_edge = true;
                    break;
                }
            }
            if (valid_edge) {
                result.push_back({id_to_set1[i], id_to_set2[matched_j]});
            }
        }
    }
    return result;
}

//exact same as other one just runs jv not hungarian
template<class T>
Vector<Pair<T, T>> BipartiteGraph<T>::get_max_weight_matching_jv() {
    Vector<Pair<T, T>> result;
    if (num_set1 == 0 || num_set2 == 0) return result;

    prepare_hungarian_matrix();
    solve_jonker_volgenant();

    for (int i = 0; i < num_set1; ++i) {
        int matched_j = match_set1[i];
        if (matched_j >= 0 && matched_j < num_set2) {
            bool valid_edge = false;
            for (const Edge& edge : adj_list[i]) {
                if (edge.to == matched_j && edge.weight > 0.0) {
                    valid_edge = true;
                    break;
                }
            }
            if (valid_edge) {
                result.push_back({id_to_set1[i], id_to_set2[matched_j]});
            }
        }
    }
    return result;
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
