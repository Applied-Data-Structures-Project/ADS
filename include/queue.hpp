#pragma once

template <typename T>
class Queue {
private:
    T* arr;        // Dynamic array storing queue items
    int capacity;  // Maximum capacity of current array
    int head;      // Index of the front (oldest) item
    int tail;      // Index where the next item will be pushed
    int count;     // Number of items currently in the queue

    void grow() {
        int newCapacity = capacity * 2;
        T* newArr = new T[newCapacity];
        
        // Copy elements in logical order from head to tail
        for (int i = 0; i < count; i++) {
            newArr[i] = arr[(head + i) % capacity];
        }

        delete[] arr;
        arr = newArr;
        capacity = newCapacity;
        head = 0;
        tail = count;
    }

public:
    Queue(int initialCapacity = 10) : capacity(initialCapacity), head(0), tail(0), count(0) {
        if (capacity < 1) capacity = 10;
        arr = new T[capacity];
    }

    Queue(const Queue& other) : capacity(other.capacity), head(0), tail(other.count), count(other.count) {
        arr = new T[capacity];
        for (int i = 0; i < count; i++) {
            arr[i] = other.arr[(other.head + i) % other.capacity];
        }
    }

    Queue& operator=(const Queue& other) {
        if (this != &other) {
            delete[] arr;
            capacity = other.capacity;
            count = other.count;
            head = 0;
            tail = count;
            arr = new T[capacity];
            for (int i = 0; i < count; i++) {
                arr[i] = other.arr[(other.head + i) % other.capacity];
            }
        }
        return *this;
    }

    ~Queue() {
        delete[] arr;
    }

    void push(const T& val) {
        if (count == capacity) {
            grow();
        }
        arr[tail] = val;
        tail = (tail + 1) % capacity;
        count++;
    }

    void pop() {
        if (count == 0) return;
        head = (head + 1) % capacity;
        count--;
    }

    T& front() {
        return arr[head];
    }

    const T& front() const {
        return arr[head];
    }

    bool empty() const {
        return count == 0;
    }

    int size() const {
        return count;
    }

    void clear() {
        head = 0;
        tail = 0;
        count = 0;
    }
};