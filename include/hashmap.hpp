
// HashMap<Index, Value>
//
// Our hash map stores pairs like "Malak" -> 0 and can find any key in O(1) time
// on average. The key and the value are both templates, so we use the same class
// everywhere in the project: the graph uses it to turn a name into a vertex ID,
// and we use it to find candidates, like all the universities that offer a major.
//
// How it works:
// The map is an array of buckets. Each key goes into bucket hash(key) % capacity.
// If two different keys end up in the same bucket (a collision), they are kept
// in a linked list in that bucket.
//
// Why we built it this way:
// - We used chaining (a linked list per bucket) because deleting a key is simple:
//   we just unlink its node. With open addressing we would need to mark deleted
//   slots, and the table could never be more than full.
// - When the table would become more than 75% full, we double its size and move
//   every key to its new bucket. This keeps the lists short, so lookups stay fast.
// - New keys are added at the front of the list, because that takes O(1) time and
//   the order inside a list doesn't matter.
// - The class has a destructor, a copy constructor and an assignment operator
//  , so copying a map gives it its own nodes and no memory
//   is leaked or freed twice.
#ifndef HASHMAP_HPP
#define HASHMAP_HPP
#include <iostream>
#include <functional>
#include <string>
#include <stdexcept>
using namespace std;
template <typename Index,typename Value>
class HashMap {
public:
    HashMap(int cap = 10);   // cap = starting number of boxes in the table
    ~HashMap();              // frees all nodes and the table


    HashMap(const HashMap& other);
    HashMap& operator=(const HashMap& other);
    Value& operator[](const Index& key);
    const Value& operator[](const Index& key) const;

    bool search(const Index& key) const;                 // true if key is found
    void insert(const Index& key, const Value& value);  // add key, or update its value
    Value* get(const Index& key);                        // it is used with graphs to get the value of a key
    const Value* get(const Index& key) const;
    Value& at(const Index& key);
    const Value& at(const Index& key) const;
    bool remove(const Index& key);                       // true if the key was found and removed
    int size() const;                                     // number of keys stored
    double loadFactor() const;                            // count / capacity
    void printTable() const;

    template <typename Func>
    void forEach(Func visit) const;                       // applies visit to every (key, value) pair

private:
    struct HashNode {
        Index key;
        Value value;
        HashNode* next;
        HashNode(const Index& k, const Value& v) : key(k), value(v), next(nullptr) {}
    };

    HashNode** table;   // array of boxes; each box points to the first node of its list
    int capacity;       // number of boxes in the table
    int count;          // number of keys stored

    int hash(const Index& key) const;
    void rehash();                       // doubles the table and moves every node
};

#include "hashmap.tpp"   // function bodies

#endif
