// Placeholder test so the test pipeline works from day one.
// Every tests/*.cpp file becomes its own CTest executable automatically.
// A test passes when main() returns 0; return non-zero (or crash) to fail.
// Delete this file once real tests exist (e.g. test_matching.cpp, test_hashmap.cpp).

#include "vector.hpp"

int main() {
    Vector<int> v;
    v.push_back(1);
    return v.size() == 1 ? 0 : 1;
}

