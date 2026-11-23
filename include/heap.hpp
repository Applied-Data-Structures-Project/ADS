// Generic binary heap (priority queue) stored in an array.
// The best item is always at index 0, so pop() gives the next best candidate.
// Think of it as a waiting list: if someone is matched or passes, pop again.
//
// Rule: every item has equal or higher priority than its children.
//   parent of i = (i-1)/2, left child = 2i+1, right child = 2i+2
//
// Heap<T> is a max-heap (biggest on top).
// Heap<T, MinCompare<T> > is a min-heap (smallest on top).
//
// push, pop, updateAt, removeAt are O(log n). peek is O(1).
// buildFromArray is O(n). Searching for an item is O(n).
//
// Equal scores can come out in any order. PriorityItem (at the bottom)
// fixes this: if scores tie, the smaller seq (earlier arrival) wins.

#pragma once

template <typename T>
struct MinCompare {
    bool operator()(const T& first, const T& second) const {
        // smaller value goes closer to the top
        return first < second;
    }
};

template <typename T>
struct MaxCompare {
    bool operator()(const T& first, const T& second) const {
        // bigger value goes closer to the top
        return first > second;
    }
};

template <typename T, typename Compare = MaxCompare<T> >
class Heap {
private:
    T* items;                // the array that stores the heap
    int count;               // how many items are in the heap now
    int capacity;            // how many items the array can hold
    Compare higherPriority;  // tells us which of two items goes higher

    void grow();
    void siftUp(int index);
    void siftDown(int index);

public:
    Heap(int initialCapacity = 16);
    Heap(const Heap& other);
    Heap& operator=(const Heap& other);
    ~Heap();
    int size() const;
    bool isEmpty() const;
    void push(const T& item);
    bool peek(T& result) const;
    bool pop(T& result);
    bool getAt(int index, T& result) const;
    bool updateAt(int index, const T& newValue);
    bool removeAt(int index);
    void buildFromArray(const T* array, int length);
    void clear();
};

#include "heap.tpp"   // function bodies (PriorityItem below does not need them)

// Ready-made item for waiting lists, with first come, first served on ties.
struct PriorityItem {
    int id;           // student, driver, course section, etc.
    double score;     // higher score = higher priority
    long long seq;    // arrival number, smaller = arrived earlier

    PriorityItem() : id(0), score(0.0), seq(0) {}
    PriorityItem(int i, double s, long long q) : id(i), score(s), seq(q) {}

    // MaxCompare uses ">", so this decides who goes above whom
    bool operator>(const PriorityItem& other) const {
        if (score != other.score) return score > other.score;
        return seq < other.seq;   // same score: earlier arrival wins
    }
};
