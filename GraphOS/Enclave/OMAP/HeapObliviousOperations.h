#ifndef OBLIVIOUSOPERATIONS_H
#define OBLIVIOUSOPERATIONS_H

#include <vector>
#include <cassert>
#include <stdlib.h>
#include <array>
#include "DOHEAP.hpp"

using namespace std;

class HeapObliviousOperations {
private:
    static void bitonic_sort(vector<HeapNode*>* nodes, int low, int n, int dir);
    static void bitonic_merge(vector<HeapNode*>* nodes, int low, int n, int dir);
    static void compare_and_swap(HeapNode* item_i, HeapNode* item_j, int dir);
    static int greatest_power_of_two_less_than(int n);

public:
    static long long INF;
    HeapObliviousOperations();
    virtual ~HeapObliviousOperations();
    static void oblixmergesort(std::vector<HeapNode*> *data);
    static void bitonicSort(vector<HeapNode*>* nodes);
    static void compaction(std::vector<HeapNode*>* data);

};

#endif /* OBLIVIOUSOPERATIONS_H */

