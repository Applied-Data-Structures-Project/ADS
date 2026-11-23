// vector.tpp: the bodies of the functions declared in vector.hpp.
// Templates must be visible wherever they are used, so vector.hpp includes
// this file at its end instead of compiling it as a separate .cpp.
// Do not include this file directly; include "vector.hpp".

template <class T>
Vector<T>::Vector() : arr(nullptr), s(0), c(0) {}

template <class T>
Vector<T>::Vector(int size) : s(size), c(size)
{
    arr = (c > 0) ? new T[c] : nullptr;
    for (int i = 0; i < s; ++i)
        arr[i] = T();
}

template <class T>
Vector<T>::Vector(int size, const T &val) : s(size), c(size)
{
    arr = (c > 0) ? new T[c] : nullptr;
    for (int i = 0; i < s; ++i)
        arr[i] = val;
}

template <class T>
Vector<T>::Vector(const Vector &other) : s(other.s), c(other.s)
{
    arr = (c > 0) ? new T[c] : nullptr;
    for (int i = 0; i < s; ++i)
        arr[i] = other.arr[i];
}

template <class T>
Vector<T>::~Vector() { delete[] arr; }

template <class T>
T *Vector<T>::begin() { return arr; }

template <class T>
T *Vector<T>::end() { return arr + s; }

template <class T>
const T *Vector<T>::begin() const { return arr; }

template <class T>
const T *Vector<T>::end() const { return arr + s; }

template <class T>
Vector<T> &Vector<T>::operator=(const Vector &other)
{
    if (this != &other)
    {
        delete[] arr;
        s = other.s;
        c = other.s;
        arr = (c > 0) ? new T[c] : nullptr;
        for (int i = 0; i < s; ++i)
            arr[i] = other.arr[i];
    }
    return *this;
}
template <class T>
T &Vector<T>::operator[](const int &idx)
{
    return arr[idx];
}

template <class T>
const T &Vector<T>::operator[](const int &idx) const
{
    return arr[idx];
}

template <class T>
T &Vector<T>::at(const int &idx)
{
    if (idx < s && idx >= 0)
        return arr[idx];
    throw std::out_of_range("Index is out of bounds");
}

template <class T>
int Vector<T>::size() const
{
    return s;
}

template <class T>
int Vector<T>::capacity() const
{
    return c;
}

template <class T>
void Vector<T>::erase()
{
    for (int i = 0; i < s; ++i)
        arr[i] = T();
    s = 0;
}

template <class T>
void Vector<T>::reserve(const int &new_cap)
{
    if (new_cap <= c)
        return;

    T *new_arr = new T[new_cap];
    for (int i = 0; i < s; ++i)
    {
        new_arr[i] = std::move(arr[i]); // Single pass copy
    }
    delete[] arr;
    arr = new_arr;
    c = new_cap;
}

template <class T>
void Vector<T>::resize(const int &size)
{
    if (size > c)
    {
        reserve(size);
    }
    for (int i = s; i < size; ++i)
    {
        arr[i] = T();
    }
    s = size;
}

template <class T>
void Vector<T>::push_back(const T &item)
{
    if (s == c)
    {
        reserve(c == 0 ? 1 : c * 2);
    }
    arr[s++] = item;
}

template <class T>
void Vector<T>::assign(const int &size, const T &val)
{
    if (size > c)
    {
        delete[] arr;
        c = size;
        arr = new T[c];
    }
    s = size;
    for (int i = 0; i < s; ++i)
    {
        arr[i] = val;
    }
}
