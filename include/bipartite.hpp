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

template<class T1,class T2>
BipartiteGraph<T1,T2>::BipartiteGraph(): num_set1(0), num_set2(0){}

template<class T1, class T2>
void BipartiteGraph<T1, T2>::add_node_set1(T1 item) {
    set1_ids[item] = num_set1++;
    id_to_set1.push_back(item);
    adj_list.push_back(Vector<Edge>()); // adds an empty list for this new node
}

template<class T1, class T2>
void BipartiteGraph<T1, T2>::add_node_set2(T2 item) {
    set2_ids[item] = num_set2++;
    id_to_set2.push_back(item);
    adj_list.push_back(Vector<Edge>()); // adds an empty list for this new node
}

template<class T1,class T2>
void BipartiteGraph<T1,T2>::add_edge(T1 set1_item, T2 set2_item, double weight){
    int set1_id, set2_id;
    try {
        set1_id = set1_ids.at(set1_item);
    } catch (const std::out_of_range& e) {
        add_node_set1(set1_item);
        set1_id = set1_ids.at(set1_item);
    }
    try {
        set2_id = set2_ids.at(set2_item);
    } catch (const std::out_of_range& e) {
        add_node_set2(set2_item);
        set2_id = set2_ids.at(set2_item);
    }

    adj_list[set1_id].push_back({set2_id, weight});
}

template<class T1,class T2>
bool BipartiteGraph<T1,T2>::hk_bfs(){
    dist.assign(num_set1, HK_INF);

    // Using our custom Queue class instead of Vector + head index
    Queue<int> queue;

    // Every free set1 node starts at layer 0
    for (int u = 0; u < num_set1; ++u) {
        if (match_set1[u] == -1) {
            dist[u] = 0;
            queue.push(u); // push into Queue
        }
    }

    int shortest = HK_INF; // layer of the first free set2 node we reach
    bool found = false;

    while (!queue.empty()) {
        int u = queue.front(); // get item at front of queue
        queue.pop();           // remove front item

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
                queue.push(w); // push into Queue
            }
        }
    }
    return found;
}

template<class T1,class T2>
bool BipartiteGraph<T1,T2>::hk_dfs(int set1_id){
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

template<class T1,class T2>
Vector<Pair<T1,T2>> BipartiteGraph<T1,T2>::get_max_cardinality_matching() {
    Vector<Pair<T1, T2>> result;
    if (num_set1 == 0 || num_set2 == 0) return result;

    // Reset matching arrays
    match_set1.assign(num_set1, -1);
    match_set2.assign(num_set2, -1);

    // ==========================================
    // 1. GREEDY WARM-UP PHASE
    // ==========================================
    for (int u = 0; u < num_set1; ++u) {
        for (int k = 0; k < (int)adj_list[u].size(); ++k) {
            int v = adj_list[u][k].to;
            
            // If internship 'v' is free, pair student 'u' with 'v' immediately
            if (match_set2[v] == -1) {
                match_set1[u] = v;
                match_set2[v] = u;
                break; // Student 'u' is matched, stop checking their other edges
            }
        }
    }

    // ==========================================
    // 2. MAIN HOPCROFT-KARP LOOP
    // ==========================================
    // Runs BFS/DFS phases only for nodes that greedy missed or couldn't pair easily
    while (hk_bfs()) {
        for (int u = 0; u < num_set1; ++u) {
            if (match_set1[u] == -1) {
                hk_dfs(u);
            }
        }
    }

    // Post-processing: collect matched pairs
    for (int u = 0; u < num_set1; ++u) {
        if (match_set1[u] != -1) {
            result.push_back({id_to_set1[u], id_to_set2[match_set1[u]]});
        }
    }
    return result;
}

template<class T1,class T2>
void BipartiteGraph<T1, T2>::prepare_hungarian_matrix() {
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

template<class T1,class T2>
void BipartiteGraph<T1, T2>::solve_hungarian() {
    int K = cost_matrix.size();
    if (K == 0) return;

    const double INF = std::numeric_limits<double>::infinity();

    // we use u and v to store how much delta weve subtracted from each column/row
    //so we dont have to update cost matrix
    // these start at 1 not 0 as 0 is dummy row and column
    Vector<double> u(K + 1, 0.0); //rows
    Vector<double> v(K + 1, 0.0); // columns
    Vector<int> p(K + 1, 0);   // this stores which row has claimed this column p[column] = row
    Vector<int> way(K + 1, 0); // helps us make the switch if 2 rows want the same column

    for (int i = 1; i <= K; ++i) {
        p[0] = i;
        int j0 = 0; // dummy column 0
        Vector<double> minv(K + 1, INF);
        Vector<bool> used(K + 1, false);

        do {
            used[j0] = true;
            int i0 = p[j0];
            double delta = INF;
            int j1 = 0;

            //loop through each column for this row and find min val so we can create a zero
            for (int j = 1; j <= K; ++j) {
                if (!used[j]) {
                    double cur = cost_matrix[i0 - 1][j - 1] - u[i0] - v[j];
                    if (cur < minv[j]) {
                        minv[j] = cur;
                        // this is new lowest cost for this column so we mark it
                        way[j] = j0;
                    }
                    if (minv[j] < delta) {
                        delta = minv[j];
                        j1 = j;
                    }
                }
            }

            // create our 0s using the mins we calculated
            for (int j = 0; j <= K; ++j) {
                if (used[j]) {
                    u[p[j]] += delta;
                    // need to update both so 0s remain and dont go negative
                    v[j] -= delta;
                } else {
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while (p[j0] != 0);

        // if a shift is required, follow all the second options
        // and shift them till everyone has 0
        do {
            int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0 != 0);
    }

    // we store final matches (with dummy nodes)
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
// but is a lot longer and more complicated, we can decide whether to keep or not later. Something like carpooling would probably
// need it as you'd run and update a lot. This implentation is from online.


// template<class T1,class T2>
// void BipartiteGraph<T1, T2>::solve_jonker_volgenant() {
//     int K = cost_matrix.size();
//     if (K == 0) return;

//     const double INF = std::numeric_limits<double>::infinity();
//     const double EPS = 1e-9;

//     Vector<double> v(K, 0.0);    // Column dual potentials
//     match_set1.assign(K, -1);    // Row to Col match
//     match_set2.assign(K, -1);    // Col to Row match
//     Vector<int> matches(K, 0);   // Count of rows assigned to each col
//     Vector<int> free_rows;       // Unassigned rows

//     // Phase 1: Column Reduction (find initial min in each column)
//     for (int j = K - 1; j >= 0; --j) {
//         double min_val = cost_matrix[0][j];
//         int i_min = 0;
//         for (int i = 1; i < K; ++i) {
//             if (cost_matrix[i][j] < min_val) {
//                 min_val = cost_matrix[i][j];
//                 i_min = i;
//             }
//         }
//         v[j] = min_val;
//         matches[i_min]++;
//         if (match_set1[i_min] == -1) {
//             match_set1[i_min] = j;
//             match_set2[j] = i_min;
//         } else if (v[j] < v[match_set1[i_min]]) {
//             match_set2[match_set1[i_min]] = -1;
//             match_set1[i_min] = j;
//             match_set2[j] = i_min;
//         }
//     }

//     // Phase 2: Reduction Transfer (adjust potentials for uniquely matched rows)
//     for (int i = 0; i < K; ++i) {
//         if (matches[i] == 0) {
//             free_rows.push_back(i);
//         } else if (matches[i] == 1) {
//             int j1 = match_set1[i];
//             double min_val = INF;
//             for (int j = 0; j < K; ++j) {
//                 if (j != j1) {
//                     double red_cost = cost_matrix[i][j] - v[j];
//                     if (red_cost < min_val) {
//                         min_val = red_cost;
//                     }
//                 }
//             }
//             v[j1] -= min_val;
//         }
//     }

//     // Phase 3: Augmenting Row Reduction (two rounds of greedy conflict resolution)
//     for (int loop = 0; loop < 2; ++loop) {
//         Vector<int> next_free;
//         for (int idx = 0; idx < (int)free_rows.size(); ++idx) {
//             int i = free_rows[idx];
//             double u1 = INF, u2 = INF;
//             int j1 = 0, j2 = -1;

//             // Find best (j1) and second-best (j2) columns for row i
//             for (int j = 0; j < K; ++j) {
//                 double h = cost_matrix[i][j] - v[j];
//                 if (h < u2) {
//                     if (h >= u1) {
//                         u2 = h;
//                         j2 = j;
//                     } else {
//                         u2 = u1;
//                         u1 = h;
//                         j2 = j1;
//                         j1 = j;
//                     }
//                 }
//             }

//             int i0 = match_set2[j1];
//             if (u1 < u2 - EPS) {
//                 v[j1] = v[j1] - (u2 - u1);
//             } else if (i0 != -1 && j2 != -1) {
//                 j1 = j2;
//                 i0 = match_set2[j1];
//             }

//             if (i0 != -1) {
//                 if (u1 < u2 - EPS) {
//                     // Re-evaluate displaced row immediately
//                     free_rows[idx] = i0;
//                     idx--;
//                 } else {
//                     next_free.push_back(i0);
//                 }
//             }
//             match_set1[i] = j1;
//             match_set2[j1] = i;
//         }
//         free_rows = next_free;
//     }

//     // Phase 4: Augmentation (Shortest Augmenting Path for remaining free rows)
//     Vector<double> d(K, INF);
//     Vector<int> pred(K, -1);
//     Vector<int> col_list(K);

//     for (int f = 0; f < (int)free_rows.size(); ++f) {
//         int free_row = free_rows[f];
//         for (int j = 0; j < K; ++j) {
//             d[j] = cost_matrix[free_row][j] - v[j];
//             pred[j] = free_row;
//             col_list[j] = j;
//         }

//         int low = 0, up = 0;
//         int unassigned_col = -1;
//         double min_d = 0.0;

//         // Dijkstra search with 3-way partitioned col_list (scanned, tight, remaining)
//         while (unassigned_col == -1) {
//             if ( up == low ) {
//                 min_d = d[col_list[up]];
//                 for (int k = up + 1; k < K; ++k) {
//                     int j = col_list[k];
//                     if (d[j] <= min_d) {
//                         if (d[j] < min_d) {
//                             up = low;
//                             min_d = d[j];
//                         }
//                         col_list[k] = col_list[up];
//                         col_list[up++] = j;
//                     }
//                 }
//                 for (int k = low; k < up; ++k) {
//                     if (match_set2[col_list[k]] == -1) {
//                         unassigned_col = col_list[k];
//                         break;
//                     }
//                 }
//             }

//             if (unassigned_col == -1) {
//                 int j1 = col_list[low++];
//                 int i = match_set2[j1];
//                 double u1 = cost_matrix[i][j1] - v[j1] - min_d;

//                 for (int k = up; k < K; ++k) {
//                     int j = col_list[k];
//                     double h = cost_matrix[i][j] - v[j] - u1;
//                     if (h < d[j]) {
//                         d[j] = h;
//                         pred[j] = i;
//                         if (std::abs(h - min_d) <= EPS) {
//                             if (match_set2[j] == -1) {
//                                 unassigned_col = j;
//                                 break;
//                             }
//                             col_list[k] = col_list[up];
//                             col_list[up++] = j;
//                         }
//                     }
//                 }
//             }
//         }

//         // Update column potentials for scanned columns
//         for (int k = 0; k < low; ++k) {
//             int j = col_list[k];
//             v[j] += d[j] - min_d;
//         }

//         // Augment matching along the shortest path
//         int j = unassigned_col;
//         int i = -1;
//         do {
//             i = pred[j];
//             match_set2[j] = i;
//             int next_j = match_set1[i];
//             match_set1[i] = j;
//             j = next_j;
//         } while (i != free_row);
//     }
// }

template<class T1,class T2>
Vector<Pair<T1, T2>> BipartiteGraph<T1, T2>::get_max_weight_matching() {
    Vector<Pair<T1, T2>> result;
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

//exact same as other one just runs jv not hungarian if we plan to use it

// template<class T1,class T2>
// Vector<Pair<T1, T2>> BipartiteGraph<T1, T2>::get_max_weight_matching_jv() {
//     Vector<Pair<T1, T2>> result;
//     if (num_set1 == 0 || num_set2 == 0) return result;

//     prepare_hungarian_matrix();
//     solve_jonker_volgenant();

//     for (int i = 0; i < num_set1; ++i) {
//         int matched_j = match_set1[i];
//         if (matched_j >= 0 && matched_j < num_set2) {
//             bool valid_edge = false;
//             for (const Edge& edge : adj_list[i]) {
//                 if (edge.to == matched_j && edge.weight > 0.0) {
//                     valid_edge = true;
//                     break;
//                 }
//             }
//             if (valid_edge) {
//                 result.push_back({id_to_set1[i], id_to_set2[matched_j]});
//             }
//         }
//     }
//     return result;
// }

template<class T1,class T2>
void BipartiteGraph<T1, T2>::debug_print() {
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
