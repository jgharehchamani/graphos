#include "HeapObliviousOperations.h"
#include "../Enclave.h"
#include "Enclave_t.h"

HeapObliviousOperations::HeapObliviousOperations() {
}

HeapObliviousOperations::~HeapObliviousOperations() {
}

void HeapObliviousOperations::oblixmergesort(std::vector<HeapNode*>* data) {
    if (data->size() == 0 || data->size() == 1) {
        return;
    }
    int len = data->size();
    int t = ceil(log2(len));
    long long p = 1 << (t - 1);

    while (p > 0) {
        long long q = 1 << (t - 1);
        long long r = 0;
        long long d = p;

        while (d > 0) {
            long long i = 0;
            while (i < len - d) {
                if ((i & p) == r) {
                    long long j = i + d;
                    if (i != j) {
                        int node_cmp = HeapNode::CTcmp((*data)[j]->evictionNode, (*data)[i]->evictionNode);
                        int dummy_blocks_last = HeapNode::CTcmp((*data)[i]->isDummy, (*data)[j]->isDummy);
                        int same_nodes = HeapNode::CTeq(node_cmp, 0);
                        bool cond = HeapNode::CTeq(HeapNode::conditional_select(dummy_blocks_last, node_cmp, same_nodes), -1);
                        HeapNode::conditional_swap((*data)[i], (*data)[j], cond);
                    }
                }
                i += 1;
            }
            d = q - p;
            q /= 2;
            r = p;
        }
        p /= 2;
    }
    std::reverse(data->begin(), data->end());
}

int HeapObliviousOperations::greatest_power_of_two_less_than(int n) {
    int k = 1;
    while (k > 0 && k < n) {
        k = k << 1;
    }
    return k >> 1;
}

void HeapObliviousOperations::bitonicSort(vector<HeapNode*>* nodes) {
    int len = nodes->size();
    bitonic_sort(nodes, 0, len, 1);
}

void HeapObliviousOperations::bitonic_sort(vector<HeapNode*>* nodes, int low, int n, int dir) {
    if (n > 1) {
        int middle = n / 2;
        bitonic_sort(nodes, low, middle, !dir);
        bitonic_sort(nodes, low + middle, n - middle, dir);
        bitonic_merge(nodes, low, n, dir);
    }
}

void HeapObliviousOperations::bitonic_merge(vector<HeapNode*>* nodes, int low, int n, int dir) {
    if (n > 1) {
        int m = greatest_power_of_two_less_than(n);

        for (int i = low; i < (low + n - m); i++) {
            if (i != (i + m)) {
                compare_and_swap((*nodes)[i], (*nodes)[i + m], dir);
            }
        }

        bitonic_merge(nodes, low, m, dir);
        bitonic_merge(nodes, low + m, n - m, dir);
    }
}

void HeapObliviousOperations::compare_and_swap(HeapNode* item_i, HeapNode* item_j, int dir) {
    int res = HeapNode::CTcmp(item_i->evictionNode, item_j->evictionNode);
    int cmp = HeapNode::CTeq(res, 1);
    HeapNode::conditional_swap(item_i, item_j, HeapNode::CTeq(cmp, dir));
}

void HeapObliviousOperations::compaction(std::vector<HeapNode*>* data) {
    int fillPosition = 0;
    vector<int> compactionLeft;

    int size = data->size();
    for (unsigned int i = 0; i < size; i++) {
        bool cond = !HeapNode::CTeq(HeapNode::CTcmp((*data)[i]->evictionNode, (long long) - 1), 0);
        compactionLeft.push_back(HeapNode::conditional_select((int) (i - fillPosition), (int) 0, cond));
        fillPosition = HeapNode::conditional_select(fillPosition + 1, fillPosition, cond);
    }

    int levels = (int) ceil(log2(size));

    for (int i = 0; i < levels; i++) {
        int power = 1 << i + 1;
        for (unsigned int j = 0; j < size; j++) {
            bool cond = !(HeapNode::CTeq(HeapNode::CTcmp((*data)[j]->evictionNode, -1), 0)) && (!HeapNode::CTeq(HeapNode::CTcmp(compactionLeft[j], 0), 0));
            int dj = compactionLeft[j];
            int val = (dj % power);
            HeapNode::conditional_swap(((*data)[j - val]), ((*data)[j]), cond);
            compactionLeft[j - val] = HeapNode::conditional_select(dj - val, compactionLeft[j - val], cond);
        }
    }

}