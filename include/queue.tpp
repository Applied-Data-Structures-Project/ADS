// queue.tpp: the bodies of the functions declared in queue.hpp.
// Templates must be visible wherever they are used, so queue.hpp includes
// this file at its end instead of compiling it as a separate .cpp.
// Do not include this file directly; include "queue.hpp".

template <typename T>
void Queue<T>::grow() {
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

template <typename T>
Queue<T>::Queue(int initialCapacity) : capacity(initialCapacity), head(0), tail(0), count(0) {
    if (capacity < 1) capacity = 10;
    arr = new T[capacity];
}

template <typename T>
Queue<T>::Queue(const Queue& other) : capacity(other.capacity), head(0), tail(other.count), count(other.count) {
    arr = new T[capacity];
    for (int i = 0; i < count; i++) {
        arr[i] = other.arr[(other.head + i) % other.capacity];
    }
}

template <typename T>
Queue<T>& Queue<T>::operator=(const Queue& other) {
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

template <typename T>
Queue<T>::~Queue() {
    delete[] arr;
}

template <typename T>
void Queue<T>::push(const T& val) {
    if (count == capacity) {
        grow();
    }
    arr[tail] = val;
    tail = (tail + 1) % capacity;
    count++;
}

template <typename T>
void Queue<T>::pop() {
    if (count == 0) return;
    head = (head + 1) % capacity;
    count--;
}

template <typename T>
T& Queue<T>::front() {
    return arr[head];
}

template <typename T>
const T& Queue<T>::front() const {
    return arr[head];
}

template <typename T>
bool Queue<T>::empty() const {
    return count == 0;
}

template <typename T>
int Queue<T>::size() const {
    return count;
}

template <typename T>
void Queue<T>::clear() {
    head = 0;
    tail = 0;
    count = 0;
}
