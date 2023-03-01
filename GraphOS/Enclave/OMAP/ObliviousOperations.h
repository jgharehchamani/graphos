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
    static void bitonic_sort(int low, int n, int dir);
    static void bitonic_merge(vector<Node*>* nodes, int low, int n, int dir);
    static void bitonic_merge(int low, int n, int dir);
    static void compare_and_swap(Node* item_i, Node* item_j, int dir);
    static int greatest_power_of_two_less_than(int n);
    static void setNode(int index, Node* node);
    static Node* getNode(int index);

public:
    static long long INF;
    static long long storeSingleBlockSize;
    static long long single_block_clen_size;
    static unsigned long long blockSize;
    static size_t single_block_plaintext_size;
    static bytes<Key> key;

    static void fetchBatch1(int beginIndex);
    static void fetchBatch2(int beginIndex);

    static void flushCache();
    static vector<Node*> setupCache1;
    static vector<Node*> setupCache2;
    static unsigned long long currentBatchBegin1;
    static unsigned long long currentBatchBegin2;
    static bool isLeftBatchUpdated;
    static unsigned long long totalNumberOfNodes;

    ObliviousOperations();
    virtual ~ObliviousOperations();
    static void oblixmergesort(std::vector<Node*> *data);
    static void bitonicSort(vector<Node*>* nodes);
    static void bitonicSort(unsigned long long len);

};

#endif /* OBLIVIOUSOPERATIONS_H */

