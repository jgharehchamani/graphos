#ifndef OBLIVIOUSOPERATIONS_H
#define OBLIVIOUSOPERATIONS_H

#include <vector>
#include <cassert>
#include <stdlib.h>
#include <array>
#include "ORAM.hpp"

using namespace std;

class ObliviousOperations {
private:
    static void bitonic_sort(vector<Node*>* nodes, int low, int n, int dir);
    static void bitonic_merge(vector<Node*>* nodes, int low, int n, int dir);
    static void compare_and_swap(Node* item_i, Node* item_j, int dir);
    static int greatest_power_of_two_less_than(int n);

public:
    static long long INF;
    ObliviousOperations();
    virtual ~ObliviousOperations();
    static void oblixmergesort(std::vector<Node*> *data);
    static void bitonicSort(vector<Node*>* nodes);
    static void compaction(std::vector<Node*>* data);

};

#endif /* OBLIVIOUSOPERATIONS_H */

