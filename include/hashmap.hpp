#ifndef HASHMAP_HPP
#define HASHMAP_HPP

#include <iostream>
#include <string>
#include <utility>
using namespace std;
//// HashMap
// This is our hash map and  It stores pairs like
// "Bob" -> 1, where the key is always a string and the value can be any type as we made it a template.
//
// Inside, it's an array of boxes. Each key goes in box hash(key) % capacity,
// and if two keys land in the same box they share a linked list there.
// When the table gets more than 75% full, it doubles and moves everything over.
//
// In our project we use it for things like student name -> ID.
// so the graph can find anyone by name quickly.

template <typename Value>
class HashMap {
public:
    HashMap(int cap = 10);   // cap = starting number of boxes in the table
    ~HashMap();              // frees all nodes and the table


    HashMap(const HashMap& other);
    HashMap& operator=(const HashMap& other);

    bool search(const string& key) const;                 // true if key is found
    void insert(const string& key, const Value& value);  // add key, or update its value
    Value* get(const string& key);                        // it is used with graphs to get the value of a key
    bool remove(const string& key);                       // true if the key was found and removed
    int size() const;                                     // number of keys stored
    double loadFactor() const;                            // count / capacity
    void printTable() const;

    template <typename Func>
    void forEach(Func visit) const;                       // applies visit to every (key, value) pair

private:
    struct HashNode {
        string key;
        Value value;
        HashNode* next;
        HashNode(string k, Value v) : key(k), value(v), next(nullptr) {}
    };

    HashNode** table;   // array of boxes; each box points to the first node of its list
    int capacity;       // number of boxes in the table
    int count;          // number of keys stored

    int hash(const string& key) const;
    void rehash();                       // doubles the table and moves every node
};

#include "hashmap.cpp"

#endif