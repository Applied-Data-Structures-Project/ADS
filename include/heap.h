// Generic binary heap (priority queue) stored in an array.
// It always gives us the highest priority item first, so we can use it for any matchmaking idea.

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

    void grow() {
        // make a bigger array, copy everything, delete the old one
        int newCapacity = capacity * 2;
        T* newItems = new T[newCapacity];
        for (int index = 0; index < count; index++) {
            newItems[index] = items[index];
        }
        delete[] items;
        items = newItems;
        capacity = newCapacity;
    }

    void siftUp(int index) {
        // keep swapping with the parent while we have higher priority
        while (index > 0) {
            int parentIndex = (index - 1) / 2;
            if (higherPriority(items[index], items[parentIndex])) {
                T temporary = items[index];
                items[index] = items[parentIndex];
                items[parentIndex] = temporary;
                index = parentIndex;
            } else {
                break;
            }
        }
    }

    void siftDown(int index) {
        // keep swapping with the stronger child while a child has higher priority
        while (true) {
            int leftIndex = 2 * index + 1;
            int rightIndex = 2 * index + 2;
            int bestIndex = index;

            // check the left child (only if it exists)
            if (leftIndex < count && higherPriority(items[leftIndex], items[bestIndex])) {
                bestIndex = leftIndex;
            }
            // check the right child (only if it exists)
            if (rightIndex < count && higherPriority(items[rightIndex], items[bestIndex])) {
                bestIndex = rightIndex;
            }
            // no child is better so the item is in the right place
            if (bestIndex == index) {
                break;
            }

            T temporary = items[index];
            items[index] = items[bestIndex];
            items[bestIndex] = temporary;
            index = bestIndex;
        }
    }

public:
    Heap(int initialCapacity = 16) {
        // the array must hold at least one item
        if (initialCapacity < 1) {
            initialCapacity = 1;
        }
        capacity = initialCapacity;
        count = 0;
        items = new T[capacity];
    }

    Heap(const Heap& other) {
        // copy constructor: make our own array so two heaps never share memory
        capacity = other.capacity;
        count = other.count;
        higherPriority = other.higherPriority;
        items = new T[capacity];
        for (int index = 0; index < count; index++) {
            items[index] = other.items[index];
        }
    }

    Heap& operator=(const Heap& other) {
        // assignment: skip if it is the same heap otherwise copy like above
        if (this != &other) {
            delete[] items;
            capacity = other.capacity;
            count = other.count;
            higherPriority = other.higherPriority;
            items = new T[capacity];
            for (int index = 0; index < count; index++) {
                items[index] = other.items[index];
            }
        }
        return *this;
    }

    ~Heap() {
        delete[] items;
    }

    int size() const {
        return count;
    }

    bool isEmpty() const {
        return count == 0;
    }

    void push(const T& item) {
        // make room if the array is full
        if (count == capacity) {
            grow();
        }
        // put the item at the end then move it up to its place
        items[count] = item;
        siftUp(count);
        count++;
    }

    bool peek(T& result) const {
        // nothing to look at in an empty heap
        if (count == 0) {
            return false;
        }
        // the top item is always at index 0
        result = items[0];
        return true;
    }

    bool pop(T& result) {
        // nothing to remove from an empty heap
        if (count == 0) {
            return false;
        }
        // save the top item to give back
        result = items[0];
        // move the last item to the top then move it down to its place
        count--;
        items[0] = items[count];
        siftDown(0);
        return true;
    }

    bool getAt(int index, T& result) const {
        // reads the item at an array position (used to search the heap)
        if (index < 0 || index >= count) {
            return false;
        }
        result = items[index];
        return true;
    }

    bool updateAt(int index, const T& newValue) {
        if (index < 0 || index >= count) {
            return false;
        }
        // overwrite the item then fix the heap in both directions
        // (only one of the two will actually move something)
        items[index] = newValue;
        siftUp(index);
        siftDown(index);
        return true;
    }

    bool removeAt(int index) {
        if (index < 0 || index >= count) {
            return false;
        }
        count--;
        // if we removed the last item there is nothing to fix
        if (index == count) {
            return true;
        }
        // fill the empty spot with the last item then fix the heap
        items[index] = items[count];
        siftUp(index);
        siftDown(index);
        return true;
    }

    void buildFromArray(const T* array, int length) {
        // throw away the old content and allocate a new array
        delete[] items;
        capacity = length;
        if (capacity < 1) {
            capacity = 1;
        }
        items = new T[capacity];
        count = length;
        for (int index = 0; index < length; index++) {
            items[index] = array[index];
        }
        // fix the heap from the last parent up to the root
        for (int index = count / 2 - 1; index >= 0; index--) {
            siftDown(index);
        }
    }

    void clear() {
        // just forget the items the array stays allocated
        count = 0;
    }
};
