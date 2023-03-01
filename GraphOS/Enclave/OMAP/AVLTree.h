#ifndef AVLTREE_H
#define AVLTREE_H
#include "ORAM.hpp"
#include <functional>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <array>
#include <memory>
#include <type_traits>
#include <iomanip>
#include "Bid.h"
#include <random>
#include "sgx_trts.h"
#include <algorithm>
#include <stdlib.h>
#include "PRF.h"
using namespace std;

class AVLTree {
private:

    int maxOfRandom;
    bool doubleRotation;

    int max(int a, int b);
    Node* newNode(Bid key, string value);
    void rotate(Node* node, Node* oppositeNode, int targetHeight, bool right, bool dummy = false);
    unsigned long long RandomPath();

    /**
     * constant time comparator
     * @param left
     * @param right
     * @return left < right -> -1,  left = right -> 0, left > right -> 1
     */
    static int CTcmp(long long lhs, long long rhs) {
        unsigned __int128 overflowing_iff_lt = (__int128) lhs - (__int128) rhs;
        unsigned __int128 overflowing_iff_gt = (__int128) rhs - (__int128) lhs;
        int is_less_than = (int) -(overflowing_iff_lt >> 127); // -1 if self < other, 0 otherwise.
        int is_greater_than = (int) (overflowing_iff_gt >> 127); // 1 if self > other, 0 otherwise.
        int result = is_less_than + is_greater_than;
        return result;
    }

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> a , choice = 0 -> return b
     */
    static long long conditional_select(long long a, long long b, int choice) {
        unsigned long long one = 1;
        return (~((unsigned long long) choice - one) & a) | ((unsigned long long) (choice - one) & b);
    }

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> a , choice = 0 -> return b
     */
    static int conditional_select(int a, int b, int choice) {
        unsigned int one = 1;
        return (~((unsigned int) choice - one) & a) | ((unsigned int) (choice - one) & b);
    }

    static bool CTeq(int a, int b) {
        return !(a^b);
    }

    static bool CTeq(long long a, long long b) {
        return !(a^b);
    }

    int sortedArrayToBST(vector<Node*>* nodes, long long start, long long end, unsigned long long& pos, Bid& node, map<unsigned long long, unsigned long long>* permutation); //small enlave setup
    int sortedArrayToBST(vector<Node*>* nodes, long long start, long long end, unsigned long long& pos, Bid& node); //The Big enclave with ready to use key-value pairs
    unsigned long long permutationIterator = 0;
    unsigned long long INF = 92233720368547758;

    Node* readWriteCacheNode(Bid bid, Node* node, bool isRead, bool isDummy);
    void bitonic_sort(vector<Node*>* nodes, int low, int n, int dir);
    void bitonic_merge(vector<Node*>* nodes, int low, int n, int dir);
    void compare_and_swap(Node* item_i, Node* item_j, int dir);
    void bitonicSort(vector<Node*>* nodes);

    void bitonic_sortOCallBased(int low, int n, int dir);
    void bitonic_mergeOCallBased(int low, int n, int dir);
    void bitonicSortOCallBased(int len);
    Node* getNode(int index);
    void setNode(int index, Node* node);

    void prfbitonic_sortOCallBased(int low, int n, int dir);
    void prfbitonic_mergeOCallBased(int low, int n, int dir);
    void prfbitonicSortOCallBased(int len);
    PRF* getPRF(int index);
    void setPRF(int index, PRF* node);
    void prf_compare_and_swap(PRF* item_i, PRF* item_j, int dir);

    int greatest_power_of_two_less_than(int n);

    void fetchPRF1(int beginIndex);
    void fetchPRF2(int beginIndex);
    void fetchBatch1(int beginIndex);
    void fetchBatch2(int beginIndex);
    void createPermutation(int maxSize);
    int sortedArrayToBST(long long start, long long end, unsigned long long& pos, Bid& node);

    void flushCache();
    void flushPRFCache();
    vector<PRF*> prfCache1;
    vector<PRF*> prfCache2;
    vector<Node*> setupCache1;
    vector<Node*> setupCache2;
    unsigned long long storeSingleBlockSize;
    unsigned long long totalNumberOfNodes;
    unsigned long long totalNumberOfPRFs;
    unsigned long long clen_size;
    unsigned long long currentPRFBatch1 = 0;
    unsigned long long currentPRFBatch2 = 0;
    unsigned long long currentBatchBegin1 = 0;
    unsigned long long currentBatchBegin2 = 0;
    bool isLeftBatchUpdated = false;
    bool isPRFLeftBatchUpdated = false;

    bytes<Key> secretkey;
    unsigned long long buckCoutner = 0;

public:
    AVLTree(long long maxSize, bytes<Key> secretkey, Bid& rootKey, unsigned long long& rootPos, map<Bid, string>* pairs, map<unsigned long long, unsigned long long>* permutation);
    AVLTree(long long maxSize, bytes<Key> key, bool isEmptyMap);
    AVLTree(long long maxSize, long long initialSize, bytes<Key> secretkey, Bid& rootKey, unsigned long long& rootPos);
    virtual ~AVLTree();
    int totheight = 0;
    vector<Node*> avlCache;
    unsigned long long PRPCnt = 0;
    unsigned long long getSamplePermutation();
    bool exist;
    unsigned long long index = 1;
    ORAM *oram;
    Bid insert(Bid rootKey, unsigned long long& pos, Bid key, string value, int& height, Bid lastID, bool isDummyIns = false);
    void searchInsert(Node* head, Bid omapKey, string& res, string newValue);
    //    Node* search(Node* head, Bid key, int newPos = -1);
    string search(Node* head, Bid key);
    void printTree(Node* root, int indent);
    void startOperation(bool batchWrite = false);
    void setupInsert(Bid& rootKey, unsigned long long& rootPos, map<Bid, string>& pairs);
    void searchAndIncrement(Node* rootNode, Bid omapKey, string& res, bool isFirstPart);
    void readAndSetDist(Node* head, Bid omapKey, string& res, string newValue);
    void finishOperation();
};

#endif /* AVLTREE_H */




