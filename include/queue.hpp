#pragma once

// Circular-array queue (first in, first out).
// push, pop, front, empty and size are O(1); push is O(1) amortized because
// grow() doubles the array only when it is full.
template <typename T>
class Queue {
private:
    T* arr;        // Dynamic array storing queue items
    int capacity;  // Maximum capacity of current array
    int head;      // Index of the front (oldest) item
    int tail;      // Index where the next item will be pushed
    int count;     // Number of items currently in the queue

    void grow();   // doubles the array, keeping the items in order

public:
    Queue(int initialCapacity = 10);
    Queue(const Queue& other);
    Queue& operator=(const Queue& other);
    ~Queue();

    void push(const T& val);
    void pop();
    T& front();
    const T& front() const;
    bool empty() const;
    int size() const;
    void clear();
};

#include "queue.tpp"   // function bodies
