#pragma once
#include <stdexcept>

//Note: I only added the functions that make vector work for now, so add more functions as we need them.
template<class T>
class Vector {
public:
    Vector() : s(0), c(0), arr(nullptr) {}

    explicit Vector(int size) : s(size), c(size) {
        arr = (c > 0) ? new T[c] : nullptr;
        for (int i = 0; i < s; ++i) arr[i] = T();
    }

    Vector(int size, const T& val) : s(size), c(size) {
        arr = (c > 0) ? new T[c] : nullptr;
        for (int i = 0; i < s; ++i) arr[i] = val;
    }

    Vector(const Vector& other) : s(other.s), c(other.s) {
        arr = (c > 0) ? new T[c] : nullptr;
        for (int i = 0; i < s; ++i) arr[i] = other.arr[i];
    }
    ~Vector(){delete [] arr;}

    T* begin() { return arr;}
    T* end()   { return arr + s;}
    const T* begin() const { return arr;}
    const T* end()   const { return arr + s;}

    Vector& operator=(const Vector& other);
    T& operator[](const int& idx);
    const T& operator[](const int& idx) const;
    T& at(const int& idx);

    int size();
    int capacity();
    void erase();
    void reserve(const int& new_cap);
    void resize(const int& size);
    void push_back(const T& item);
    void assign(const int& size, const T& val);
private:
    T *arr;
    int s; //size
    int c; //capacity
};

template<class T>
Vector<T>& Vector<T>::operator=(const Vector& other){
    if (this != &other) {
        delete[] arr;
        s = other.s;
        c = other.s;
        arr = (c > 0) ? new T[c] : nullptr;
        for (int i = 0; i < s; ++i) arr[i] = other.arr[i];
    }
    return *this;
}
template<class T>
T& Vector<T>::operator[](const int& idx) {
    return arr[idx];
}

template<class T>
const T& Vector<T>::operator[](const int& idx) const{
    return arr[idx];
}

template<class T>
T& Vector<T>::at(const int& idx) {
    if (idx<s && idx>=0)
        return arr[idx];
    throw std::out_of_range("Index is out of bounds");
}

template<class T>
int Vector<T>::size() {
    return s;
}

template<class T>
int Vector<T>::capacity() {
    return c;
}

template<class T>
void Vector<T>::erase() {
    for (int i=0; i<s; ++i) arr[i] = T();
}

template<class T>
void Vector<T>::reserve(const int& new_cap) {
    if (new_cap <= c) return;

    T* new_arr = new T[new_cap];
    for (int i = 0; i < s; ++i) {
        new_arr[i] = std::move(arr[i]); // Single pass copy
    }
    delete[] arr;
    arr = new_arr;
    c = new_cap;
}

template<class T>
void Vector<T>::resize(const int& size) {
    if (size > c) {
        reserve(size);
    }
    for (int i = s; i < size; ++i) {
        arr[i] = T();
    }
    s = size;
}

template<class T>
void Vector<T>::push_back(const T& item) {
    if (s == c) {
        reserve(c == 0 ? 1 : c * 2);
    }
    arr[s++] = item;
}

template<class T>
void Vector<T>::assign(const int& size, const T& val) {
    if (size > c) {
        delete[] arr;
        c = size;
        arr = new T[c];
    }
    s = size;
    for (int i = 0; i < s; ++i) {
        arr[i] = val;
    }
}

