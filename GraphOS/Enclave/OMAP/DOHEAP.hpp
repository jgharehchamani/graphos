#ifndef DOHEAP_H
#define DOHEAP_H

#include "AES.hpp"
#include <random>
#include <vector>
#include <unordered_map>
#include <string>
#include <iostream>
#include <map>
#include <set>
#include "Bid.h"
#include "LocalRAMStore.hpp"

using namespace std;

class HeapNode {
public:

    HeapNode() {
    }

    ~HeapNode() {
    }
    unsigned long long index;
    std::array< byte_t, 16> value;
    Bid key;
    unsigned long long pos;
    long long evictionNode;
    bool isDummy;
    //    int compactionLeft;
    std::array< byte_t, 64> dum;

    static HeapNode* clone(HeapNode* oldNode) {
        HeapNode* newNode = new HeapNode();
        newNode->evictionNode = oldNode->evictionNode;
        newNode->index = oldNode->index;
        newNode->pos = oldNode->pos;
        newNode->value = oldNode->value;
        newNode->key = oldNode->key;
        newNode->isDummy = oldNode->isDummy;
        newNode->dum = oldNode->dum;
        return newNode;
    }

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

    static unsigned long long conditional_select(unsigned long long a, unsigned long long b, int choice) {
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
    static unsigned __int128 conditional_select(unsigned __int128 a, unsigned __int128 b, int choice) {
        unsigned __int128 one = 1;
        return (~((unsigned __int128) choice - one) & a) | ((unsigned __int128) (choice - one) & b);
    }

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> a , choice = 0 -> return b
     */
    static byte_t conditional_select(byte_t a, byte_t b, int choice) {
        byte_t one = 1;
        return (~((byte_t) choice - one) & a) | ((byte_t) (choice - one) & b);
    }

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> a , choice = 0 -> return b
     */
    static void conditional_swap(HeapNode* a, HeapNode* b, int choice) {
        HeapNode tmp = *b;
        for (int k = 0; k < b->value.size(); k++) {
            b->value[k] = HeapNode::conditional_select(a->value[k], b->value[k], choice);
        }
        for (int k = 0; k < b->key.id.size(); k++) {
            b->key.id[k] = HeapNode::conditional_select(a->key.id[k], b->key.id[k], choice);
        }
        b->index = HeapNode::conditional_select((long long) a->index, (long long) b->index, choice);
        b->isDummy = HeapNode::conditional_select(a->isDummy, b->isDummy, choice);
        b->pos = HeapNode::conditional_select((long long) a->pos, (long long) b->pos, choice);
        for (int k = 0; k < b->dum.size(); k++) {
            b->dum[k] = HeapNode::conditional_select(a->dum[k], b->dum[k], choice);
        }
        b->evictionNode = HeapNode::conditional_select(a->evictionNode, b->evictionNode, choice);
        a->index = HeapNode::conditional_select((long long) tmp.index, (long long) a->index, choice);
        for (int k = 0; k < b->value.size(); k++) {
            a->value[k] = HeapNode::conditional_select(tmp.value[k], a->value[k], choice);
        }
        for (int k = 0; k < a->key.id.size(); k++) {
            a->key.id[k] = HeapNode::conditional_select(tmp.key.id[k], a->key.id[k], choice);
        }
        a->isDummy = HeapNode::conditional_select(tmp.isDummy, a->isDummy, choice);
        a->pos = HeapNode::conditional_select((long long) tmp.pos, (long long) a->pos, choice);
        for (int k = 0; k < b->dum.size(); k++) {
            a->dum[k] = HeapNode::conditional_select(tmp.dum[k], a->dum[k], choice);
        }
        a->evictionNode = HeapNode::conditional_select(tmp.evictionNode, a->evictionNode, choice);
    }

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> b->a , choice = 0 -> return a->a
     */
    static void conditional_assign(HeapNode* a, HeapNode* b, int choice) {
        a->index = HeapNode::conditional_select((long long) b->index, (long long) a->index, choice);
        a->isDummy = HeapNode::conditional_select(b->isDummy, a->isDummy, choice);
        a->pos = HeapNode::conditional_select((long long) b->pos, (long long) a->pos, choice);
        for (int k = 0; k < b->value.size(); k++) {
            a->value[k] = HeapNode::conditional_select(b->value[k], a->value[k], choice);
        }
        for (int k = 0; k < b->dum.size(); k++) {
            a->dum[k] = HeapNode::conditional_select(b->dum[k], a->dum[k], choice);
        }
        a->evictionNode = HeapNode::conditional_select(b->evictionNode, a->evictionNode, choice);
        for (int k = 0; k < a->key.id.size(); k++) {
            a->key.id[k] = HeapNode::conditional_select(b->key.id[k], a->key.id[k], choice);
        }
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

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> a , choice = 0 -> return b
     */
    static unsigned int conditional_select(unsigned int a, unsigned int b, int choice) {
        unsigned int one = 1;
        return (~((unsigned int) choice - one) & a) | ((unsigned int) (choice - one) & b);
    }

    static bool CTeq(int a, int b) {
        return !(a^b);
    }

    static bool CTeq(long long a, long long b) {
        return !(a^b);
    }

    static bool CTeq(unsigned __int128 a, unsigned __int128 b) {
        return !(a^b);
    }

    static bool CTeq(unsigned long long a, unsigned long long b) {
        return !(a^b);
    }
};

struct HeapBlock {
    unsigned long long id;
    block data;
};

class HeapBucket {
public:
    std::array<HeapBlock, Z> blocks;
    HeapBlock subtree_min;
};

class HeapCache {
public:
    vector<HeapNode*> nodes;

    void preAllocate(int n) {
        nodes.reserve(n);
    }

    void insert(HeapNode* node) {
        nodes.push_back(node);
    };
};

class DOHEAP {
private:

    unsigned int PERMANENT_STASH_SIZE;

    size_t blockSize;
    unordered_map<long long, HeapBucket> virtualStorage;
    HeapCache stash;
    long long currentLeaf;

    bytes<Key> key;
    size_t plaintext_size;
    long long bucketCount;
    size_t clen_size;
    bool batchWrite = false;
    long long maxOfRandom;
    long long maxHeightOfAVLTree;
    LocalRAMStore* localStore;
    bool useLocalRamStore = false;
    int storeBlockSize;


    long long GetNodeOnPath(long long leaf, int depth);

    void FetchPath(long long leaf);

    block SerialiseBucket(HeapBucket bucket);
    HeapBucket DeserialiseBucket(block buffer);

    vector<HeapBucket> ReadBuckets(vector<long long> indexes);
    void InitializeBuckets(long long strtindex, long long endindex, HeapBucket bucket);
    void WriteBuckets(vector<long long> indexes, vector<HeapBucket> buckets);
    void EvictBuckets();
    void UpdateMin();



    bool WasSerialised();
    HeapNode* convertBlockToNode(block b);
    block convertNodeToBlock(HeapNode* node);
    void WriteBucket(long long index, HeapBucket bucket);

public:
    DOHEAP(long long maxSize, bytes<Key> key, bool simulation);
    DOHEAP(long long maxSize, bytes<Key> oram_key, vector<HeapNode*>* nodes, map<unsigned long long, unsigned long long> permutation);
    ~DOHEAP();
    double evicttime = 0;
    int evictcount = 0;
    int readCnt = 0;
    int depth;
    unsigned long long INF, nextDummyCounter;
    vector<vector<double> > times;
    bool beginProfile = false;

    unsigned long long RandomPath();
    void start(bool batchWrite);
    void insert(Bid k, array< byte_t, 16> v);
    pair<Bid,array<byte_t, 16> > extractMin();
    array< byte_t, 16> findMin();
    void dummy();
    pair<Bid,array<byte_t, 16> > execute(Bid k, array<byte_t, 16> v, int op);
    void evict(bool evictBuckets = false);
    bool profile = false;
};

#endif
