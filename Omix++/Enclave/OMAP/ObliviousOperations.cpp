#include "ObliviousOperations.h"
#include "../Enclave.h"
#include "Enclave_t.h"

ObliviousOperations::ObliviousOperations() {
}

ObliviousOperations::~ObliviousOperations() {
}

void ObliviousOperations::oblixmergesort(std::vector<Node*>* data) {
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
                        int node_cmp = Node::CTcmp((*data)[j]->evictionNode, (*data)[i]->evictionNode);
                        int dummy_blocks_last = Node::CTcmp((*data)[i]->isDummy, (*data)[j]->isDummy);
                        int same_nodes = Node::CTeq(node_cmp, 0);
                        bool cond = Node::CTeq(Node::conditional_select(dummy_blocks_last, node_cmp, same_nodes), -1);
                        Node::conditional_swap((*data)[i], (*data)[j], cond);
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

int ObliviousOperations::greatest_power_of_two_less_than(int n) {
    int k = 1;
    while (k > 0 && k < n) {
        k = k << 1;
    }
    return k >> 1;
}

void ObliviousOperations::bitonicSort(vector<Node*>* nodes) {
    int len = nodes->size();
    bitonic_sort(nodes, 0, len, 1);
}

void ObliviousOperations::bitonic_sort(vector<Node*>* nodes, int low, int n, int dir) {
    if (n > 1) {
        int middle = n / 2;
        bitonic_sort(nodes, low, middle, !dir);
        bitonic_sort(nodes, low + middle, n - middle, dir);
        bitonic_merge(nodes, low, n, dir);
    }
}

void ObliviousOperations::bitonic_merge(vector<Node*>* nodes, int low, int n, int dir) {
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

void ObliviousOperations::compare_and_swap(Node* item_i, Node* item_j, int dir) {
    int res = Node::CTcmp(item_i->evictionNode, item_j->evictionNode);
    int cmp = Node::CTeq(res, 1);
    Node::conditional_swap(item_i, item_j, Node::CTeq(cmp, dir));
}
