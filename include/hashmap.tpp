// hashmap.tpp: the bodies of the functions declared in hashmap.hpp.
// Templates must be visible wherever they are used, so hashmap.hpp includes
// this file at its end instead of compiling it as a separate .cpp.
// Do not include this file directly; include "hashmap.hpp".

// ---------- hash formulas ----------

// Polynomial hash for text (emails, slugs, "email|docType").
// Every letter changes the number, and multiplying by 31 makes the order of the
// letters matter, so "ab" and "ba" get different numbers.
// Example: "ab" -> h = 0*31 + 97 = 97, then h = 97*31 + 98 = 3105.
// 31 is a small prime, which spreads the keys well across the buckets.
// unsigned long wraps around on overflow, which is fine for hashing.
inline unsigned long hashKey(const std::string& key)
{
    unsigned long h = 0;
    for (int i = 0; i < (int)key.size(); i++)
    {
        h = h * 31 + (unsigned char)key[i];
    }
    return h;
}

// Number keys (like a letter id): the number itself is already spread out.
inline unsigned long hashKey(int key)
{
    return (unsigned long)key;
}

// Any other key type: use C++'s built-in hash function.
template <typename Key>
unsigned long hashKey(const Key& key)
{
    return std::hash<Key>{}(key);
}

template <typename Index,typename Value>
HashMap<Index,Value>::HashMap(int cap) : capacity(cap), count(0)
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

template <typename Index,typename Value>
HashMap<Index,Value>::HashMap(const HashMap& other)
    : table(nullptr), capacity(other.capacity), count(0)
{
    table = new HashNode *[capacity](); // new empty boxes, same size as other

    for (int i = 0; i < capacity; i++)
    {
        HashNode *last = nullptr; // last node we copied into this box

            HashNode *current = other.table[i];
            while (current != nullptr)
            {
                HashNode *newNode = new HashNode(current->key, current->value);

            if (last == nullptr)
            {
                table[i] = newNode;     // first node: attach it to the box
            }
            else
            {
                last->next = newNode;   // later nodes: attach after the last one (keeps the same order)
            }
            last = newNode;
            count++;

            current = current->next;
        }
    }
}

template <typename Index,typename Value>
HashMap<Index,Value>& HashMap<Index,Value>::operator=(const HashMap& other)
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

template <typename Index, typename Value>
Value& HashMap<Index, Value>::operator[](const Index& key)
{
    Value* ptr = get(key);
    if (ptr == nullptr)
    {
        insert(key, Value());
        ptr = get(key);
    }
    return *ptr;
}

template <typename Index, typename Value>
const Value& HashMap<Index, Value>::operator[](const Index& key) const
{
    return at(key);
}

template <typename Index,typename Value>
HashMap<Index,Value>::~HashMap()
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

template <typename Index,typename Value>
int HashMap<Index,Value>::hash(const Index &key) const
{
    unsigned long hash_value = hashKey(key);   // our formula (see hashKey above)
    return hash_value % capacity;               // the remainder is the bucket number
}

template <typename Index,typename Value>
bool HashMap<Index,Value>::search(const Index &key) const
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

template <typename Index,typename Value>
void HashMap<Index,Value>::insert(const Index &key, const Value &value)
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

template <typename Index,typename Value>
void HashMap<Index,Value>::rehash()
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

template <typename Index,typename Value>
Value *HashMap<Index,Value>::get(const Index &key)
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

template <typename Index,typename Value>
const Value *HashMap<Index,Value>::get(const Index &key) const
{
    int index = hash(key);
    HashNode *current = table[index];
    while (current != nullptr)
    {
        if (current->key == key)
        {
            return &(current->value);
        }
        current = current->next;
    }
    return nullptr;
}

template <typename Index,typename Value>
Value &HashMap<Index,Value>::at(const Index &key)
{
    Value *ptr = get(key);
    if (!ptr)
    {
        throw out_of_range("Key not found");
    }
    return *ptr;
}

template <typename Index,typename Value>
const Value &HashMap<Index,Value>::at(const Index &key) const
{
    const Value *ptr = get(key);
    if (!ptr)
    {
        throw out_of_range("Key not found");
    }
    return *ptr;
}

template <typename Index,typename Value>
bool HashMap<Index,Value>::remove(const Index &key)
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

template <typename Index,typename Value>
int HashMap<Index,Value>::size() const
{
    return count;
}

template <typename Index,typename Value>
double HashMap<Index,Value>::loadFactor() const
{
    return static_cast<double>(count) / capacity;
}

template <typename Index,typename Value>
template <typename Func>
void HashMap<Index,Value>::forEach(Func visit) const
{
    for (int i = 0; i < capacity; i++)
    {
        const HashNode *current = table[i];
        while (current != nullptr)
        {
            visit(current->key, current->value);
            current = current->next;
        }
    }
}

template <typename Index,typename Value>
void HashMap<Index,Value>::printTable() const
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
