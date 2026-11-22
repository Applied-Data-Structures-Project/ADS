// test_mergesort.cpp
// Unit tests for generic stable mergeSort (include/mergesort.hpp).

#include <cstdio>
#include <string>

#include "mergesort.hpp"
#include "vector.hpp"
#include "test_helpers.hpp"

using namespace std;

struct TaggedItem {
    int key = 0;
    int seq = 0;
    TaggedItem() = default;
    TaggedItem(int k, int s) : key(k), seq(s) {}
};

int main() {
    printf("Basic sorting\n");
    Vector<int> empty;
    mergeSort(empty, [](const int& a, const int& b) { return a < b; });
    CHECK(empty.size() == 0, "empty vector stays empty");

    Vector<int> single;
    single.push_back(42);
    mergeSort(single, [](const int& a, const int& b) { return a < b; });
    CHECK(single.size() == 1 && single[0] == 42, "single element stays intact");

    Vector<int> nums;
    int input[] = {5, 1, 9, 3, 7, 2, 8, 4, 6, 0};
    for (int v : input) nums.push_back(v);
    mergeSort(nums, [](const int& a, const int& b) { return a < b; });
    bool sortedAsc = true;
    for (int i = 0; i < 10; i++) {
        if (nums[i] != i) sortedAsc = false;
    }
    CHECK(sortedAsc, "sorts 10 integers in ascending order");

    mergeSort(nums, [](const int& a, const int& b) { return a > b; });
    bool sortedDesc = true;
    for (int i = 0; i < 10; i++) {
        if (nums[i] != 9 - i) sortedDesc = false;
    }
    CHECK(sortedDesc, "sorts 10 integers in descending order");

    printf("Stability\n");
    Vector<TaggedItem> items;
    items.push_back(TaggedItem(2, 1));
    items.push_back(TaggedItem(1, 2));
    items.push_back(TaggedItem(2, 3));
    items.push_back(TaggedItem(1, 4));
    items.push_back(TaggedItem(2, 5));
    mergeSort(items, [](const TaggedItem& a, const TaggedItem& b) { return a.key < b.key; });

    CHECK(items[0].key == 1 && items[0].seq == 2 &&
          items[1].key == 1 && items[1].seq == 4 &&
          items[2].key == 2 && items[2].seq == 1 &&
          items[3].key == 2 && items[3].seq == 3 &&
          items[4].key == 2 && items[4].seq == 5,
          "equal keys preserve their original arrival order (stable)");

    if (failures > 0) {
        printf("SOME FAILED (%d failed)\n", failures);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
