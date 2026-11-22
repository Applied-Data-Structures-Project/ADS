#pragma once

// Generic stable merge sort for our custom Vector<T>.
// O(n log n) worst-case time, O(n) auxiliary space, no STL containers.
//
// `comesBefore(a, b)` returns true when `a` must appear strictly before `b`.
// Stability: if neither comesBefore(a, b) nor comesBefore(b, a) is true (a tie),
// the element from the left half is placed first, preserving original order.

#include "vector.hpp"

namespace ads_detail {

template <typename T, typename Compare>
void mergeRange(Vector<T>& items, Vector<T>& scratch, int left, int mid, int right, Compare comesBefore) {
    int i = left;
    int j = mid;
    int k = left;

    while (i < mid && j < right) {
        // Stable: take right ONLY when right strictly comes before left.
        if (comesBefore(items[j], items[i])) {
            scratch[k++] = items[j++];
        } else {
            scratch[k++] = items[i++];
        }
    }
    while (i < mid) scratch[k++] = items[i++];
    while (j < right) scratch[k++] = items[j++];

    for (int idx = left; idx < right; idx++) {
        items[idx] = scratch[idx];
    }
}

template <typename T, typename Compare>
void mergeSortRange(Vector<T>& items, Vector<T>& scratch, int left, int right, Compare comesBefore) {
    if (right - left <= 1) return;
    int mid = left + (right - left) / 2;
    mergeSortRange(items, scratch, left, mid, comesBefore);
    mergeSortRange(items, scratch, mid, right, comesBefore);
    mergeRange(items, scratch, left, mid, right, comesBefore);
}

} // namespace ads_detail

template <typename T, typename Compare>
void mergeSort(Vector<T>& items, Compare comesBefore) {
    if (items.size() <= 1) return;
    Vector<T> scratch(items.size());
    ads_detail::mergeSortRange(items, scratch, 0, items.size(), comesBefore);
}
