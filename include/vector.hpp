#pragma once
#include <stdexcept>
#include <utility>
// Note: I only added the functions that make vector work for now, so add more functions as we need them.
template <class T>
class Vector
{
public:
    Vector();
    explicit Vector(int size);
    Vector(int size, const T &val);
    Vector(const Vector &other);
    ~Vector();

    T *begin();
    T *end();
    const T *begin() const;
    const T *end() const;

    Vector &operator=(const Vector &other);
    T &operator[](const int &idx);
    const T &operator[](const int &idx) const;
    T &at(const int &idx);

    int size() const;
    int capacity() const;
    void erase();
    void reserve(const int &new_cap);
    void resize(const int &size);
    void push_back(const T &item);
    void assign(const int &size, const T &val);

private:
    T *arr;
    int s; // size
    int c; // capacity
};

#include "vector.tpp"   // function bodies
