#ifndef HASHMAP_CPP
#define HASHMAP_CPP

#include "hashmap.hpp"

template <typename Value>
HashMap<Value>::HashMap(int cap) : capacity(cap), count(0)
{
    if (capacity < 1)
    {
        capacity = 10; // to avoid program crashing due to divide by zero
    }
    table = new HashNode *[capacity];
    for (int i = 0; i < capacity; i++)
    {
        table[i] = nullptr;
    }
}

template <typename Value>
HashMap<Value>::~HashMap()
{
    for (int i = 0; i < capacity; i++)
    {
        HashNode *current = table[i];
        while (current != nullptr)
        {
            HashNode *temp = current;
            current = current->next;
            delete temp;
        }
    }
    delete[] table;
}
template <typename Value>
int HashMap<Value>::hash(const string &key) const
{
    unsigned long hash_value = 0; // did it unsigned so that we dont go negative
    for (char c : key)
    {
        hash_value = (hash_value * 31) + static_cast<unsigned char>(c); // used 31 because it is a prime number and helps in reducing collisions
    }
    return hash_value % capacity;
}
template <typename Value>
bool HashMap<Value>::search(const string &key) const
{
    int index = hash(key);
    HashNode *current = table[index];
    while (current != nullptr)
    { // Traverse the linked list at this index
        if (current->key == key)
        {
            return true; // Key found
        }
        current = current->next;
    }
    return false; // Key not found
}
template <typename Value>
void HashMap<Value>::insert(const string &key, const Value &value)
{
    int index = hash(key);

    // first check if the key is already in the table
    HashNode *current = table[index];
    while (current != nullptr)
    {
        if (current->key == key)
        {
            current->value = value; // key already exists so just update the value
            return;
        }
        current = current->next;
    }
    if ((double)(count + 1) / capacity > 0.75)
    {
        rehash();
        index = hash(key);
    }
    // key is not in the table so make a new node
    HashNode *newNode = new HashNode(key, value);

    if (table[index] == nullptr)
    {
        // slot is empty so the new node is the first one
        table[index] = newNode;
    }
    else
    {
        // slot already has nodes so put the new node at the front
        newNode->next = table[index];
        table[index] = newNode;
    }

    count++; // we added a new key
}
template <typename Value>
void HashMap<Value>::rehash()
{
    HashNode **oldTable = table;
    int oldCapacity = capacity;
    int newCapacity = capacity * 2; // double the capacity
    table = new HashNode *[newCapacity];
    for (int i = 0; i < newCapacity; i++)
    {
        table[i] = nullptr;
    }
    capacity = newCapacity;
    HashNode *current = nullptr; // Initialize current pointer
    for (int i = 0; i < oldCapacity; i++)
    {
        current = oldTable[i];
        while (current != nullptr)
        {
            HashNode *nextNode = current->next; // save next first
            int newIndex = hash(current->key);  // new slot (uses new capacity)
            current->next = table[newIndex];    // put it at the front
            table[newIndex] = current;
            current = nextNode;
        }
    }
    delete[] oldTable; // free the old table
}
template <typename Value>
Value *HashMap<Value>::get(const string &key)
{
int index = hash(key);
    HashNode *current = table[index];
    while (current != nullptr)
    { // Traverse the linked list at this index
        if (current->key == key)
        {
            return &(current->value); // Return pointer to the value if key is found
        }
        current = current->next;
    }
    return nullptr; // Key not found
}
template <typename Value>
bool HashMap<Value>::remove(const string &key)
{
    int index = hash(key);
    HashNode *current = table[index];
    HashNode *previous = nullptr; //  in order to Keep track of the previous node to update the next pointer

    while (current != nullptr)
    {
        if (current->key == key)
        {
            if (previous == nullptr)
            {
                table[index] = current->next; // key is at the head of the list
            }
            else
            {
                previous->next = current->next; // key is in the middle or end of the list
            }
            delete current;
            count--;
            return true;
        }
        previous = current; // Move previous to current
        current = current->next; // Move current to the next node
    }
    return false;
}
template <typename Value>
int HashMap<Value>::size() const
{
    return count;
}
template <typename Value>
double HashMap<Value>::loadFactor() const
{
    return static_cast<double>(count) / capacity;
}
 template <typename Value>
 template <typename Func>
void HashMap<Value>::forEach(Func visit) const
{
    for (int i = 0; i < capacity; i++)   // every box, including empty ones
    {
        HashNode *current = table[i];
        while (current != nullptr)       // every node in this box's list
        {
            visit(current->key, current->value);
            current = current->next;
        }
    }
}

template <typename Value>
void HashMap<Value>::printTable() const
{
    for (int i = 0; i < capacity; i++)
    {
        cout << "Box " << i << ": ";
        HashNode *current = table[i];
        if (current == nullptr)
        {
            cout << "Empty";
        }
        else
        {
            while (current != nullptr)
            {
                cout << "(" << current->key << ", " << current->value << ") -> ";
                current = current->next;
            }
            cout << "nullptr"; // end of the linked list
        }
        cout << endl;
    }
} // for testing
#endif