#ifndef HASHMAP_HPP
#define HASHMAP_HPP

#include <iostream>
#include <string>
#include <utility>
using namespace std;
//// HashMap
// This is our hash map and  It stores pairs like
// "Bob" -> 1, where the key is always a string and the value can be any type as we made it a template.
//
// Inside, it's an array of boxes. Each key goes in box hash(key) % capacity,
// and if two keys land in the same box they share a linked list there.
// When the table gets more than 75% full, it doubles and moves everything over.
//
// In our project we use it for things like student name -> ID.
// so the graph can find anyone by name quickly.

template <typename Value>
class HashMap {
public:
    HashMap(int cap = 10);   // cap = starting number of boxes in the table
    ~HashMap();              // frees all nodes and the table


    HashMap(const HashMap& other);
    HashMap& operator=(const HashMap& other);

    bool search(const string& key) const;                 // true if key is found
    void insert(const string& key, const Value& value);  // add key, or update its value
    Value* get(const string& key);                        // it is used with graphs to get the value of a key
    bool remove(const string& key);                       // true if the key was found and removed
    int size() const;                                     // number of keys stored
    double loadFactor() const;                            // count / capacity
    void printTable() const;

    template <typename Func>
    void forEach(Func visit) const;                       // applies visit to every (key, value) pair

private:
    struct HashNode {
        string key;
        Value value;
        HashNode* next;
        HashNode(string k, Value v) : key(k), value(v), next(nullptr) {}
    };

    HashNode** table;   // array of boxes; each box points to the first node of its list
    int capacity;       // number of boxes in the table
    int count;          // number of keys stored

    int hash(const string& key) const;
    void rehash();                       // doubles the table and moves every node
};

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
HashMap<Value>::HashMap(const HashMap& other)
    : table(nullptr), capacity(other.capacity), count(0)
{
    table = new HashNode *[capacity]();


        for (int i = 0; i < capacity; i++)
        {
            HashNode** tail = &table[i];
            for (HashNode* current = other.table[i]; current != nullptr; current = current->next)
            {
                *tail = new HashNode(current->key, current->value);
                tail = &((*tail)->next);
                count++;
            }
        }


}

template <typename Value>
HashMap<Value>& HashMap<Value>::operator=(const HashMap& other)
{
    if (this != &other)
    {
        HashMap copy(other);


        HashNode **tempTable = table;
        table = copy.table;
        copy.table = tempTable;

        int tempCapacity = capacity;
        capacity = copy.capacity;
        copy.capacity = tempCapacity;

        int tempCount = count;
        count = copy.count;
        copy.count = tempCount;
    }
    return *this;
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
        hash_value = (hash_value * 31) + static_cast<unsigned char>(c); // used 31 because it is a prime number and keeping each linked list short so lookups stay toward O(1)
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

        table[index] = newNode;
    }
    else
    {

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
            int newIndex = hash(current->key);
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
    for (int i = 0; i < capacity; i++)
    {
        HashNode *current = table[i];
        while (current != nullptr)
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