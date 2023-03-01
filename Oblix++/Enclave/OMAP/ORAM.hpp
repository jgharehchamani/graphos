#ifndef ORAM_H
#define ORAM_H

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

class Node {
public:

    Node() {
    }

    ~Node() {
    }
    unsigned long long index;
    unsigned long long value;
    //    std::array< byte_t, 16> value;
    unsigned long long pos;
    long long evictionNode;
    bool isDummy;
    //    int compactionLeft;
    std::array< byte_t, 88> dum;

    static Node* clone(Node* oldNode) {
        Node* newNode = new Node();
        newNode->evictionNode = oldNode->evictionNode;
        newNode->index = oldNode->index;
        newNode->pos = oldNode->pos;
        newNode->value = oldNode->value;
        newNode->isDummy = oldNode->isDummy;
        //        newNode->compactionLeft = oldNode->compactionLeft;
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
    static void conditional_swap(Node* a, Node* b, int choice) {
        Node tmp = *b;
        b->index = Node::conditional_select((long long) a->index, (long long) b->index, choice);
        b->isDummy = Node::conditional_select(a->isDummy, b->isDummy, choice);
        b->pos = Node::conditional_select((long long) a->pos, (long long) b->pos, choice);
        b->value = Node::conditional_select((long long) a->value, (long long) b->value, choice);
        for (int k = 0; k < b->dum.size(); k++) {
            b->dum[k] = Node::conditional_select(a->dum[k], b->dum[k], choice);
        }
        b->evictionNode = Node::conditional_select(a->evictionNode, b->evictionNode, choice);
        a->index = Node::conditional_select((long long) tmp.index, (long long) a->index, choice);
        a->isDummy = Node::conditional_select(tmp.isDummy, a->isDummy, choice);
        a->pos = Node::conditional_select((long long) tmp.pos, (long long) a->pos, choice);
        a->value = Node::conditional_select((long long) tmp.value, (long long) a->value, choice);
        for (int k = 0; k < b->dum.size(); k++) {
            a->dum[k] = Node::conditional_select(tmp.dum[k], a->dum[k], choice);
        }
        a->evictionNode = Node::conditional_select(tmp.evictionNode, a->evictionNode, choice);
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

struct Block {
    unsigned long long id;
    block data;
};

using Bucket = std::array<Block, Z>;

class Cache {
public:
    vector<Node*> nodes;

    void preAllocate(int n) {
        nodes.reserve(n);
    }

    void insert(Node* node) {
        nodes.push_back(node);
    };
};

class ORAM {
private:

    unsigned int PERMANENT_STASH_SIZE;

    size_t blockSize;
    unordered_map<long long, Bucket> virtualStorage;
    Cache stash;
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

    block SerialiseBucket(Bucket bucket);
    Bucket DeserialiseBucket(block buffer);

    vector<Bucket> ReadBuckets(vector<long long> indexes);
    void InitializeBuckets(long long strtindex, long long endindex, Bucket bucket);
    void WriteBuckets(vector<long long> indexes, vector<Bucket> buckets);
    void EvictBuckets();



    bool WasSerialised();
    Node* convertBlockToNode(block b);
    block convertNodeToBlock(Node* node);
    void WriteBucket(long long index, Bucket bucket);

public:
    ORAM(long long maxSize, bytes<Key> key, bool simulation);
    ORAM(long long maxSize, bytes<Key> oram_key, vector<Node*>* nodes, map<unsigned long long, unsigned long long> permutation);
    ~ORAM();
    double evicttime = 0;
    int evictcount = 0;
    int readCnt = 0;
    int depth;
    unsigned long long INF, nextDummyCounter;
    vector<vector<double> > times;
    bool beginProfile = false;

    unsigned long long RandomPath();
    unsigned long long Access(unsigned long long bid, unsigned long long pos, unsigned long long& content);
    void start(bool batchWrite);
    void prepareForEvictionTest();
    void evict(bool evictBuckets = false);
    bool profile = false;
};

#endif
