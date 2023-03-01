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
using namespace std;

class AVLTree {
private:
    ORAM *oram;
    int maxOfRandom;
    bool doubleRotation;

    int max(int a, int b);
    Node* newNode(Bid key, string value);
    void rotate(Node* node, Node* oppositeNode, int targetHeight, bool right, bool dummy = false);
    void rotate2(Node* node, Node* oppositeNode, int targetHeight, bool right, bool dummy = false);
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

    int sortedArrayToBST(vector<Node*>* nodes, long long start, long long end, unsigned long long& pos, Bid& node, map<unsigned long long, unsigned long long>* permutation);
    unsigned long long permutationIterator = 0;
    unsigned long long INF = 92233720368547758;

    Node* readWriteCacheNode(Bid bid, Node* node, bool isRead, bool isDummy);
    void bitonic_sort(vector<Node*>* nodes, int low, int n, int dir);
    void bitonic_merge(vector<Node*>* nodes, int low, int n, int dir);
    void compare_and_swap(Node* item_i, Node* item_j, int dir);
    void bitonicSort(vector<Node*>* nodes);
    int greatest_power_of_two_less_than(int n);
    
public:
    AVLTree(long long maxSize, bytes<Key> secretkey, Bid& rootKey, unsigned long long& rootPos, map<Bid, string>* pairs, map<unsigned long long, unsigned long long>* permutation);
    AVLTree(long long maxSize, bytes<Key> key,bool isEmptyMap);
    virtual ~AVLTree();
    int totheight = 0;
    bool logTime = false;
    vector<vector<double> > times;
    bool exist;
    vector<Node*> avlCache;
    unsigned long long index = 1;
    Bid insert(Bid rootKey, unsigned long long& pos, Bid key, string value, int& height, Bid lastID, bool isDummyIns = false);
    Bid insert2(Bid rootKey, unsigned long long& pos, Bid key, string value, int& height, Bid lastID, bool isDummyIns = false);
    Bid searchInsert(Bid rootKey, unsigned long long& pos, Bid key, string& value, int& height, Bid lastID, bool isDummyIns = false);
    //    Node* search(Node* head, Bid key, int newPos = -1);
    string search(Node* head, Bid key);
    void batchSearch(Node* head, vector<Bid> keys, vector<Node*>* results);
    void printTree(Node* root, int indent);
    void startOperation(bool batchWrite = false);
    void setupInsert(Bid& rootKey, int& rootPos, map<Bid, string>& pairs);
    void finishOperation();
};

#endif /* AVLTREE_H */




