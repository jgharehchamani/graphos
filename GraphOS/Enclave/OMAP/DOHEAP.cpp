#include "DOHEAP.hpp"
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <random>
#include <cmath>
#include <cassert>
#include <cstring>
#include <map>
#include <stdexcept>
#include "sgx_trts.h"
#include "HeapObliviousOperations.h"
#include "Enclave_t.h"  /* print_string */
#include "../Enclave.h"
#include <algorithm>
#include <stdlib.h>
#include <vector>

DOHEAP::DOHEAP(long long maxSize, bytes<Key> oram_key, bool simulation)
: key(oram_key) {
    depth = (int) (ceil(log2(maxSize)) - 1) + 1;
    maxOfRandom = (long long) (pow(2, depth));
    printf("Number of leaves:%lld\n", maxOfRandom);
    printf("depth:%lld\n", depth);
    AES::Setup();
    bucketCount = (long long) maxOfRandom * 2 - 1;
    INF = 9223372036854775807 - (bucketCount);
    PERMANENT_STASH_SIZE = 90;
    stash.preAllocate(PERMANENT_STASH_SIZE * 4);

    nextDummyCounter = INF;
    blockSize = sizeof (HeapNode); // B    
    printf("block size:%d\n", blockSize);
    size_t blockCount = (size_t) (Z * bucketCount);
    storeBlockSize = (size_t) (IV + AES::GetCiphertextLength((int) ((Z + 1) * (blockSize))));
    clen_size = AES::GetCiphertextLength((int) (blockSize) * (Z + 1));
    plaintext_size = (blockSize) * (Z + 1);
    if (!simulation) {
        if (useLocalRamStore) {
            localStore = new LocalRAMStore(blockCount, storeBlockSize);
        } else {
            ocall_setup_heapStore(blockCount, storeBlockSize);
        }
    } else {
        ocall_setup_heapStore(depth, -1);
    }

    maxHeightOfAVLTree = (int) floor(log2(blockCount)) + 1;
    times.push_back(vector<double>());
    times.push_back(vector<double>());
    times.push_back(vector<double>());
    times.push_back(vector<double>());
    times.push_back(vector<double>());

    printf("Initializing DOHEAP Buckets\n");
    HeapBucket bucket;
    for (int z = 0; z < Z; z++) {
        bucket.blocks[z].id = 0;
        bucket.blocks[z].data.resize(blockSize, 0);
    }
    bucket.subtree_min.id = 0;
    bucket.subtree_min.data.resize(blockSize, 0);
    if (!simulation) {
        //        InitializeBuckets(0, bucketCount, bucket);
        long long i;
        for (i = 0; i < maxOfRandom - 1; i++) {
            if (i % 10000 == 0) {
                printf("%d/%d\n", i, bucketCount);
            }
            HeapBucket bucket;
            for (int z = 0; z < Z; z++) {
                bucket.blocks[z].id = 0;
                bucket.blocks[z].data.resize(blockSize, 0);
            }
            bucket.subtree_min.id = 0;
            bucket.subtree_min.data.resize(blockSize, 0);
            WriteBucket((int) i, bucket);
        }
        for (long long j = 0; i < bucketCount; i++, j++) {
            if (i % 10000 == 0) {
                printf("%d/%d\n", i, bucketCount);
            }
            HeapBucket bucket;

            bucket.blocks[0].id = j;
            HeapNode* tmp = new HeapNode();
            tmp->index = 0;
            tmp->pos = j;
            bucket.blocks[0].data = convertNodeToBlock(tmp);
            delete tmp;
            for (int z = 1; z < Z; z++) {
                bucket.blocks[z].id = 0;
                bucket.blocks[z].data.resize(blockSize, 0);
            }
            bucket.subtree_min.id = 0;
            bucket.subtree_min.data.resize(blockSize, 0);
            WriteBucket((long long) i, bucket);
        }
    }
    for (auto i = 0; i < PERMANENT_STASH_SIZE; i++) {
        HeapNode* dummy = new HeapNode();
        dummy->index = nextDummyCounter;
        dummy->evictionNode = -1;
        dummy->isDummy = true;
        stash.insert(dummy);
        nextDummyCounter++;
    }
    printf("End of Initialization\n");
}

DOHEAP::~DOHEAP() {
    AES::Cleanup();
}

// Fetches the array index a bucket that lise on a specific path

void DOHEAP::WriteBucket(long long index, HeapBucket bucket) {
    block b = SerialiseBucket(bucket);
    block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
    ocall_write_heapStore(index, (const char*) ciphertext.data(), (size_t) ciphertext.size());
}

long long DOHEAP::GetNodeOnPath(long long leaf, int curDepth) {
    leaf += bucketCount / 2;
    for (int d = depth - 1; d >= 0; d--) {
        bool cond = !HeapNode::CTeq(HeapNode::CTcmp(d, curDepth), -1);
        leaf = HeapNode::conditional_select((leaf + 1) / 2 - 1, leaf, cond);
    }
    return leaf;
}

// Write bucket to a single block

block DOHEAP::SerialiseBucket(HeapBucket bucket) {
    block buffer;
    for (int z = 0; z < Z; z++) {
        HeapBlock b = bucket.blocks[z];
        buffer.insert(buffer.end(), b.data.begin(), b.data.end());
    }
    HeapBlock b = bucket.subtree_min;
    buffer.insert(buffer.end(), b.data.begin(), b.data.end());
    return buffer;
}

HeapBucket DOHEAP::DeserialiseBucket(block buffer) {
    HeapBucket bucket;
    for (int z = 0; z < Z; z++) {
        HeapBlock &curBlock = bucket.blocks[z];
        curBlock.data.assign(buffer.begin(), buffer.begin() + blockSize);
        HeapNode* node = convertBlockToNode(curBlock.data);
        bool cond = HeapNode::CTeq(node->index, (unsigned long long) 0);
        node->index = HeapNode::conditional_select(node->index, nextDummyCounter, !cond);
        for (int k = 0; k < node->value.size(); k++) {
            node->value[k] = HeapNode::conditional_select(node->value[k], 0, !cond);
        }
        for (int k = 0; k < node->key.id.size(); k++) {
            node->key.id[k] = HeapNode::conditional_select(node->key.id[k], 0, !cond);
        }
        node->isDummy = HeapNode::conditional_select(0, 1, !cond);
        stash.insert(node);
        buffer.erase(buffer.begin(), buffer.begin() + blockSize);
    }
    HeapBlock &curBlock = bucket.subtree_min;
    curBlock.data.assign(buffer.begin(), buffer.begin() + blockSize);
    return bucket;
}

vector<HeapBucket> DOHEAP::ReadBuckets(vector<long long> indexes) {
    vector<HeapBucket> res;
    if (indexes.size() == 0) {
        return res;
    }
    if (useLocalRamStore) {
        for (unsigned int i = 0; i < indexes.size(); i++) {
            block ciphertext = localStore->Read(indexes[i]);
            block buffer = AES::Decrypt(key, ciphertext, clen_size);
            HeapBucket bucket = DeserialiseBucket(buffer);
            res.push_back(bucket);
        }
    } else {
        size_t readSize;
        char* tmp = new char[indexes.size() * storeBlockSize];
        ocall_nread_heapStore(&readSize, indexes.size(), indexes.data(), tmp, indexes.size() * storeBlockSize);
        for (unsigned int i = 0; i < indexes.size(); i++) {
            block ciphertext(tmp + i*readSize, tmp + (i + 1) * readSize);
            block buffer = AES::Decrypt(key, ciphertext, clen_size);
            HeapBucket bucket = DeserialiseBucket(buffer);
            res.push_back(bucket);
            virtualStorage[indexes[i]] = bucket;
        }
        delete tmp;
    }
    return res;
}

void DOHEAP::InitializeBuckets(long long strtindex, long long endindex, HeapBucket bucket) {
    block b = SerialiseBucket(bucket);
    block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
    if (useLocalRamStore) {
        for (long long i = strtindex; i < endindex; i++) {
            localStore->Write(i, ciphertext);
        }
    } else {
        ocall_initialize_heapStore(strtindex, endindex, (const char*) ciphertext.data(), (size_t) ciphertext.size());
    }
}

void DOHEAP::WriteBuckets(vector<long long> indexes, vector<HeapBucket> buckets) {
    for (unsigned int i = 0; i < indexes.size(); i++) {
        if (virtualStorage.count(indexes[i]) != 0) {
            virtualStorage.erase(indexes[i]);
        }
        virtualStorage[indexes[i]] = buckets[i];
    }
}

void DOHEAP::EvictBuckets() {
    unordered_map<long long, HeapBucket>::iterator it = virtualStorage.begin();
    if (useLocalRamStore) {
        for (auto item : virtualStorage) {
            block b = SerialiseBucket(item.second);
            block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
            localStore->Write(item.first, ciphertext);
        }
    } else {
        for (unsigned int j = 0; j <= virtualStorage.size() / 10000; j++) {
            char* tmp = new char[10000 * storeBlockSize];
            vector<long long> indexes;
            size_t cipherSize = 0;
            for (int i = 0; i < min((int) (virtualStorage.size() - j * 10000), 10000); i++) {
                block b = SerialiseBucket(it->second);
                indexes.push_back(it->first);
                block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
                std::memcpy(tmp + i * ciphertext.size(), ciphertext.data(), ciphertext.size());
                cipherSize = ciphertext.size();
                it++;
            }
            if (min((int) (virtualStorage.size() - j * 10000), 10000) != 0) {
                ocall_nwrite_heapStore(min((int) (virtualStorage.size() - j * 10000), 10000), indexes.data(), (const char*) tmp, cipherSize * min((int) (virtualStorage.size() - j * 10000), 10000));
            }
            delete tmp;
            indexes.clear();
        }
    }
    virtualStorage.clear();
}
// Fetches blocks along a path, adding them to the stash

void DOHEAP::FetchPath(long long leaf) {
    readCnt++;
    vector<long long> nodesIndex;
    vector<long long> existingIndexes;

    long long node = leaf;

    node += bucketCount / 2;
    if (virtualStorage.count(node) == 0) {
        nodesIndex.push_back(node);
    } else {
        existingIndexes.push_back(node);
    }

    for (int d = depth - 1; d >= 0; d--) {
        node = (node + 1) / 2 - 1;
        if (virtualStorage.count(node) == 0) {
            nodesIndex.push_back(node);
        } else {
            existingIndexes.push_back(node);
        }
    }

    ReadBuckets(nodesIndex);

    for (unsigned int i = 0; i < existingIndexes.size(); i++) {
        HeapBucket bucket = virtualStorage[existingIndexes[i]];
        for (int z = 0; z < Z; z++) {
            HeapBlock &curBlock = bucket.blocks[z];
            HeapNode* node = convertBlockToNode(curBlock.data);
            bool cond = HeapNode::CTeq(node->index, (unsigned long long) 0);
            node->index = HeapNode::conditional_select(node->index, nextDummyCounter, !cond);
            node->isDummy = HeapNode::conditional_select(0, 1, !cond);
            stash.insert(node);
        }
    }
}

void DOHEAP::UpdateMin() {
    vector<long long> nodesIndex;

    long long node = currentLeaf;
    node += bucketCount / 2;
    for (int d = depth - 1; d >= 0; d--) {
        long long bucketID = node;
        if (virtualStorage.count(bucketID) == 0) {
            nodesIndex.push_back(bucketID);
        }
        if (bucketID % 2 == 0) {
            bucketID--;
        } else {
            bucketID++;
        }
        if (virtualStorage.count(bucketID) == 0) {
            nodesIndex.push_back(bucketID);
        }
        node = (node + 1) / 2 - 1;
    }

    if (virtualStorage.count(0) == 0) {
        nodesIndex.push_back(0);
    }
    if (nodesIndex.size() > 0) {
        size_t readSize;
        char* tmp = new char[nodesIndex.size() * storeBlockSize];
        ocall_nread_heapStore(&readSize, nodesIndex.size(), nodesIndex.data(), tmp, nodesIndex.size() * storeBlockSize);
        for (unsigned int i = 0; i < nodesIndex.size(); i++) {
            block ciphertext(tmp + i*readSize, tmp + (i + 1) * readSize);
            block buffer = AES::Decrypt(key, ciphertext, clen_size);
            HeapBucket bucket;
            for (int z = 0; z < Z; z++) {
                HeapBlock &curBlock = bucket.blocks[z];
                curBlock.data.assign(buffer.begin(), buffer.begin() + blockSize);
                buffer.erase(buffer.begin(), buffer.begin() + blockSize);
            }
            HeapBlock &curBlock = bucket.subtree_min;
            curBlock.data.assign(buffer.begin(), buffer.begin() + blockSize);
            virtualStorage[nodesIndex[i]] = bucket;
        }
        delete tmp;
    }

    node = currentLeaf;
    node += bucketCount / 2;
    for (int d = depth; d >= 0; d--) {
        HeapBucket& curBucket = virtualStorage[node];
        HeapNode localMin;
        unsigned long long localID = 0;
        localMin.key.setInfinity();
        localMin.isDummy = true;
        for (int i = 0; i < Z; i++) {
            HeapNode* node = convertBlockToNode(curBucket.blocks[i].data);
            bool cond = Bid::CTeq(1, Bid::CTcmp(localMin.key, node->key)) && !node->isDummy;
            HeapNode::conditional_assign(&localMin, node, cond);
            localID = HeapNode::conditional_select(localID, curBucket.blocks[i].id, cond);
            delete node;
        }
        if (d != depth) {
            HeapBucket leftBucket = virtualStorage[((node + 1)*2) - 1];
            HeapNode* leftnode = convertBlockToNode(leftBucket.subtree_min.data);
            bool cond = Bid::CTeq(1, Bid::CTcmp(localMin.key, leftnode->key)) && !leftnode->isDummy;
            HeapNode::conditional_assign(&localMin, leftnode, cond);
            localID = HeapNode::conditional_select(localID, leftBucket.subtree_min.id, cond);
            delete leftnode;

            HeapBucket rightBucket = virtualStorage[((node + 1)*2)];
            HeapNode* rightnode = convertBlockToNode(rightBucket.subtree_min.data);
            cond = Bid::CTeq(1, Bid::CTcmp(localMin.key, rightnode->key)) && !rightnode->isDummy;
            HeapNode::conditional_assign(&localMin, rightnode, cond);
            localID = HeapNode::conditional_select(localID, rightBucket.subtree_min.id, cond);
            delete rightnode;
        }

        curBucket.subtree_min.id = localID;
        block tmp = convertNodeToBlock(&localMin);
        for (int k = 0; k < tmp.size(); k++) {
            curBucket.subtree_min.data[k] = HeapNode::conditional_select(curBucket.subtree_min.data[k], tmp[k], localMin.isDummy);
        }

        node = (node + 1) / 2 - 1;
    }
}

pair<Bid,array<byte_t, 16> > DOHEAP::extractMin() {
    pair<Bid,array<byte_t, 16> > res;
    array<byte_t, 16> result;
    HeapBlock curBlock;
    if (virtualStorage.count(0) == 0) {
        vector<long long> nodesIndex;
        nodesIndex.push_back(0);
        size_t readSize;
        char* tmp = new char[nodesIndex.size() * storeBlockSize];
        ocall_nread_heapStore(&readSize, nodesIndex.size(), nodesIndex.data(), tmp, nodesIndex.size() * storeBlockSize);
        block ciphertext(tmp, tmp + readSize);
        block buffer = AES::Decrypt(key, ciphertext, clen_size);
        curBlock.data.assign(buffer.begin() + blockSize*Z, buffer.begin() + blockSize * (Z + 1));
        delete tmp;
    } else {
        curBlock.data.assign(virtualStorage[0].subtree_min.data.begin(), virtualStorage[0].subtree_min.data.end());
    }
    HeapNode* rootnode = convertBlockToNode(curBlock.data);
    HeapNode* minnode = new HeapNode();
    HeapNode::conditional_assign(minnode,rootnode,true);
    bool isInStash=false;
    
    for (HeapNode* node : stash.nodes) {
        isInStash = HeapNode::CTeq(-1, Bid::CTcmp(node->key, minnode->key)) && !node->isDummy;
        HeapNode::conditional_assign(minnode,node,isInStash);
    }
    
    for (int k = 0; k < minnode->value.size(); k++) {
        result[k] = minnode->value[k];
    }

    res.second = result;
    res.first = minnode->key;
    
    FetchPath(minnode->pos);
    for (HeapNode* node : stash.nodes) {
        bool choice = HeapNode::CTeq(0, Bid::CTcmp(node->key, minnode->key)) && HeapNode::CTeq(0, Bid::CTcmp(node->value, minnode->value));
        node->isDummy = HeapNode::conditional_select(true, node->isDummy, choice);
        node->index = HeapNode::conditional_select((unsigned long long) 0, node->index, choice);
    }
    currentLeaf = minnode->pos;
    delete minnode;
    delete rootnode;
    evict(true);
    EvictBuckets();
    return res;
}

array<byte_t, 16> DOHEAP::findMin() {
    array<byte_t, 16> result;
    HeapBlock curBlock;
    if (virtualStorage.count(0) == 0) {
        vector<long long> nodesIndex;
        nodesIndex.push_back(0);
        size_t readSize;
        char* tmp = new char[nodesIndex.size() * storeBlockSize];
        ocall_nread_heapStore(&readSize, nodesIndex.size(), nodesIndex.data(), tmp, nodesIndex.size() * storeBlockSize);
        block ciphertext(tmp, tmp + readSize);
        block buffer = AES::Decrypt(key, ciphertext, clen_size);
        curBlock.data.assign(buffer.begin() + blockSize*Z, buffer.begin() + blockSize * (Z + 1));
        delete tmp;
    } else {
        curBlock.data.assign(virtualStorage[0].subtree_min.data.begin(), virtualStorage[0].subtree_min.data.end());
    }
    HeapNode* minnode = convertBlockToNode(curBlock.data);
    for (int k = 0; k < minnode->value.size(); k++) {
        result[k] = minnode->value[k];
    }
    delete minnode;
    return result;
}

/**
 * 
 * @param k k[0] is least significant byte and k[16] is the most significant byte
 * @param v v[0] is least significant byte and v[16] is the most significant byte
 */
void DOHEAP::insert(Bid k, array<byte_t, 16> v) {
    HeapNode* node = new HeapNode();
    node->pos = RandomPath();
    node->key = k;
    node->value = v;
    node->index = 1;
    node->isDummy = false;
    stash.insert(node);
    currentLeaf = RandomPath() / 2;
    FetchPath(currentLeaf);
    evict(true);
    currentLeaf = RandomPath() / 2 + (maxOfRandom / 2);
    FetchPath(currentLeaf);
    evict(true);
    EvictBuckets();
}

void DOHEAP::dummy() {
    currentLeaf = RandomPath() / 2;
    FetchPath(currentLeaf);
    evict(true);
    currentLeaf = RandomPath() / 2 + (maxOfRandom / 2);
    FetchPath(currentLeaf);
    evict(true);
    EvictBuckets();
}

/**
 * @param OP:1 extract-min  2:insert    3: dummy
 * @return 
 */
pair<Bid,array<byte_t, 16> > DOHEAP::execute(Bid k, array<byte_t, 16> v, int op) {
    pair<Bid,array<byte_t, 16> > res;
    HeapNode* node = new HeapNode();
    node->pos = RandomPath();
    Bid dummyKey;
    dummyKey.setInfinity();
    bool isInsert = HeapNode::CTeq(op, 2);
    bool isExtract = HeapNode::CTeq(op, 1);
    node->key = Bid::conditional_select(k, dummyKey, isInsert);
    node->value = v;
    node->index = HeapNode::conditional_select(1, 0, isInsert);
    node->isDummy = !isInsert;
    stash.insert(node);

    array<byte_t, 16> result;
    HeapBlock curBlock;
    if (virtualStorage.count(0) == 0) {
        vector<long long> nodesIndex;
        nodesIndex.push_back(0);
        size_t readSize;
        char* tmp = new char[nodesIndex.size() * storeBlockSize];
        ocall_nread_heapStore(&readSize, nodesIndex.size(), nodesIndex.data(), tmp, nodesIndex.size() * storeBlockSize);
        block ciphertext(tmp, tmp + readSize);
        block buffer = AES::Decrypt(key, ciphertext, clen_size);
        curBlock.data.assign(buffer.begin() + blockSize*Z, buffer.begin() + blockSize * (Z + 1));
        delete tmp;
    } else {
        curBlock.data.assign(virtualStorage[0].subtree_min.data.begin(), virtualStorage[0].subtree_min.data.end());
    }
    HeapNode* rootnode = convertBlockToNode(curBlock.data);
    HeapNode* minnode = new HeapNode();
    HeapNode::conditional_assign(minnode,rootnode,true);
    bool isInStash=false;
    
    for (HeapNode* node : stash.nodes) {
        isInStash = HeapNode::CTeq(-1, Bid::CTcmp(node->key, minnode->key)) && isExtract && !node->isDummy;
        HeapNode::conditional_assign(minnode,node,isInStash);
    }  
    
    for (int k = 0; k < minnode->value.size(); k++) {
        result[k] = minnode->value[k];
    }
    res.second = result;
    res.first = minnode->key;

    currentLeaf = RandomPath() / 2;
    currentLeaf = HeapNode::conditional_select(minnode->pos, (unsigned long long) currentLeaf, isExtract);

    FetchPath(currentLeaf);
    for (HeapNode* node : stash.nodes) {
        bool choice = HeapNode::CTeq(0, Bid::CTcmp(node->key, minnode->key)) && isExtract && HeapNode::CTeq(0, Bid::CTcmp(node->value, minnode->value));
        node->isDummy = HeapNode::conditional_select(true, node->isDummy, choice);
        node->index = HeapNode::conditional_select((unsigned long long) 0, node->index, choice);
    }
    evict(true);
    currentLeaf = RandomPath() / 2 + (maxOfRandom / 2);
    FetchPath(currentLeaf);
    evict(true);
    EvictBuckets();
    delete minnode;
    delete rootnode;
    return res;
}

HeapNode* DOHEAP::convertBlockToNode(block b) {
    HeapNode* node = new HeapNode();
    std::array<byte_t, sizeof (HeapNode) > arr;
    std::copy(b.begin(), b.begin() + sizeof (HeapNode), arr.begin());
    from_bytes(arr, *node);
    node->isDummy = HeapNode::CTeq(node->index, (unsigned long long) 0);
    return node;
}

block DOHEAP::convertNodeToBlock(HeapNode* node) {
    std::array<byte_t, sizeof (HeapNode) > data = to_bytes(*node);
    block b(data.begin(), data.end());
    return b;
}

void DOHEAP::evict(bool evictBuckets) {
    double time;

    ocall_start_timer(15);
    if (profile) {
        ocall_start_timer(15);
        ocall_start_timer(10);
    }

    vector<long long> firstIndexes;
    long long tmpleaf = currentLeaf;
    tmpleaf += bucketCount / 2;
    firstIndexes.push_back(tmpleaf);

    for (int d = depth - 1; d >= 0; d--) {
        tmpleaf = (tmpleaf + 1) / 2 - 1;
        firstIndexes.push_back(tmpleaf);
    }


    for (HeapNode* node : stash.nodes) {
        long long xorVal = 0;
        xorVal = HeapNode::conditional_select((unsigned long long) 0, node->pos ^ currentLeaf, node->isDummy);
        long long indx = 0;

        indx = (long long) floor(log2(HeapNode::conditional_select(xorVal, (long long) 1, HeapNode::CTcmp(xorVal, 0))));
        indx = indx + HeapNode::conditional_select(1, 0, HeapNode::CTcmp(xorVal, 0));

        for (long long i = 0; i < firstIndexes.size(); i++) {
            bool choice = HeapNode::CTeq(i, indx);
            long long value = firstIndexes[i];
            node->evictionNode = HeapNode::conditional_select(firstIndexes[indx], node->evictionNode, !node->isDummy && choice);
        }


    }

    if (profile) {
        ocall_stop_timer(&time, 10);
        printf("Assigning stash blocks to lowest possible level:%f\n", time);
        ocall_start_timer(10);
    }

    long long node = currentLeaf + bucketCount / 2;
    for (int d = (int) depth; d >= 0; d--) {
        for (int j = 0; j < Z; j++) {
            HeapNode* dummy = new HeapNode();
            dummy->index = nextDummyCounter;
            nextDummyCounter++;
            dummy->evictionNode = node;
            dummy->isDummy = true;
            stash.nodes.push_back(dummy);
        }
        node = (node + 1) / 2 - 1;
    }

    if (profile) {
        ocall_stop_timer(&time, 10);
        printf("Creating Dummy Blocks for each HeapBucket:%f\n", time);
        ocall_start_timer(10);
    }

    if (beginProfile) {
        ocall_start_timer(10);
    }

    HeapObliviousOperations::oblixmergesort(&stash.nodes);

    if (beginProfile) {
        ocall_stop_timer(&time, 10);
        times[1].push_back(time);
        ocall_start_timer(10);
    }

    if (profile) {
        ocall_stop_timer(&time, 10);
        printf("First Oblivious Sort: %f\n", time);
        ocall_start_timer(10);
    }

    long long currentID = currentLeaf + (bucketCount / 2);
    int level = depth;
    int counter = 0;

    for (unsigned long long i = 0; i < stash.nodes.size(); i++) {
        HeapNode* curNode = stash.nodes[i];
        bool firstCond = (!HeapNode::CTeq(HeapNode::CTcmp(counter - (depth - level) * Z, Z), -1));
        bool secondCond = HeapNode::CTeq(HeapNode::CTcmp(curNode->evictionNode, currentID), 0);
        bool thirdCond = (!HeapNode::CTeq(HeapNode::CTcmp(counter, Z * depth), -1)) || curNode->isDummy;
        bool fourthCond = HeapNode::CTeq(curNode->evictionNode, currentID);

        long long tmpEvictionNode = GetNodeOnPath(currentLeaf, depth - (int) floor(counter / Z));
        long long tmpcurrentID = GetNodeOnPath(currentLeaf, level - 1);
        curNode->evictionNode = HeapNode::conditional_select((long long) - 1, curNode->evictionNode, firstCond && secondCond && thirdCond);
        curNode->evictionNode = HeapNode::conditional_select(tmpEvictionNode, curNode->evictionNode, firstCond && secondCond && !thirdCond);
        counter = HeapNode::conditional_select(counter + 1, counter, firstCond && secondCond && !thirdCond);
        counter = HeapNode::conditional_select(counter + 1, counter, !firstCond && fourthCond);
        level = HeapNode::conditional_select(level - 1, level, firstCond && !secondCond);
        i = HeapNode::conditional_select(i - 1, i, firstCond && !secondCond);
        currentID = HeapNode::conditional_select(tmpcurrentID, currentID, firstCond&&!secondCond);

        //        if (firstCond) {
        //            if (secondCond) {
        //                if (thirdCond) {
        //                    long long tmpEvictionNode = GetNodeOnPath(currentLeaf, depth - (int) floor(counter / Z));
        //                    curNode->evictionNode = -1;
        //                    counter = counter;
        //                    level = level;
        //                    i = i;
        //                } else {
        //                    long long tmpEvictionNode = GetNodeOnPath(currentLeaf, depth - (int) floor(counter / Z));
        //                    curNode->evictionNode = tmpEvictionNode;
        //                    counter++;
        //                    level = level;
        //                    i = i;
        //                }
        //            } else {
        //                currentID = GetNodeOnPath(currentLeaf, level - 1);
        //                curNode->evictionNode = curNode->evictionNode;
        //                counter = counter;
        //                level--;
        //                i--;
        //            }
        //
        //        } else if (curNode->evictionNode == currentID) {
        //            long long tmpID = GetNodeOnPath(currentLeaf, level - 1);
        //            curNode->evictionNode = curNode->evictionNode;
        //            counter++;
        //            level = level;
        //            i = i;
        //        } else {
        //            long long tmpID = GetNodeOnPath(currentLeaf, level - 1);
        //            curNode->evictionNode = curNode->evictionNode;
        //            counter = counter;
        //            level = level;
        //            i = i;
        //        }
    }

    if (beginProfile) {
        ocall_stop_timer(&time, 10);
        times[2].push_back(time);
    }

    if (profile) {
        ocall_stop_timer(&time, 10);
        printf("Sequential Scan on Stash Blocks to assign blocks to blocks:%f\n", time);
        ocall_start_timer(10);
    }

    //    HeapObliviousOperations::compaction(&stash.nodes);
    HeapObliviousOperations::oblixmergesort(&stash.nodes);

    if (profile) {
        ocall_stop_timer(&time, 10);
        printf("Oblivious Compaction: %f\n", time);
        ocall_start_timer(10);
    }

    unsigned int j = 0;
    HeapBucket* bucket = new HeapBucket();
    bucket->subtree_min.id = 0;
    bucket->subtree_min.data.resize(blockSize, 0);
    for (int i = 0; i < (depth + 1) * Z; i++) {
        HeapNode* cureNode = stash.nodes[i];
        long long curBucketID = cureNode->evictionNode;
        HeapBlock &curBlock = (*bucket).blocks[j];
        curBlock.data.resize(blockSize, 0);
        block tmp = convertNodeToBlock(cureNode);
        curBlock.id = HeapNode::conditional_select((unsigned long long) 0, cureNode->index, cureNode->isDummy);
        for (int k = 0; k < tmp.size(); k++) {
            curBlock.data[k] = HeapNode::conditional_select(curBlock.data[k], tmp[k], cureNode->isDummy);
        }
        delete cureNode;
        j++;

        if (j == Z) {
            virtualStorage.erase(curBucketID);
            virtualStorage[curBucketID] = (*bucket);
            delete bucket;
            bucket = new HeapBucket();
            bucket->subtree_min.id = 0;
            bucket->subtree_min.data.resize(blockSize, 0);
            j = 0;
        }
    }
    delete bucket;

    if (profile) {
        ocall_stop_timer(&time, 10);
        printf("Creating Buckets to write:%f\n", time);
        ocall_start_timer(10);
    }

    stash.nodes.erase(stash.nodes.begin(), stash.nodes.begin()+((depth + 1) * Z));

    for (unsigned int i = PERMANENT_STASH_SIZE; i < stash.nodes.size(); i++) {
        delete stash.nodes[i];
    }
    stash.nodes.erase(stash.nodes.begin() + PERMANENT_STASH_SIZE, stash.nodes.end());

    nextDummyCounter = INF;

    if (profile) {
        ocall_stop_timer(&time, 10);
        printf("Padding stash:%f\n", time);
        ocall_start_timer(10);
    }

    if (beginProfile) {
        ocall_start_timer(10);
    }

    UpdateMin();

    if (beginProfile) {
        ocall_stop_timer(&time, 10);
        times[3].push_back(time);
    }

    if (profile) {
        ocall_stop_timer(&time, 10);
        printf("Out of SGX memory write:%f\n", time);
    }

    evictcount++;

    if (profile) {
        ocall_stop_timer(&time, 15);
        evicttime += time;
        printf("eviction time:%f\n", time);
    }

    if (beginProfile) {
        ocall_stop_timer(&time, 15);
        times[4].push_back(time);
    }
}

void DOHEAP::start(bool isBatchWrite) {
    this->batchWrite = isBatchWrite;
    readCnt = 0;
}

unsigned long long DOHEAP::RandomPath() {
    uint32_t val;
    sgx_read_rand((unsigned char *) &val, 4);
    return val % (maxOfRandom);
}

DOHEAP::DOHEAP(long long maxSize, bytes<Key> oram_key, vector<HeapNode*>* nodes, map<unsigned long long, unsigned long long> permutation)
: key(oram_key) {
    depth = (int) (ceil(log2(maxSize)) - 1) + 1;
    maxOfRandom = (long long) (pow(2, depth));
    AES::Setup();
    bucketCount = maxOfRandom * 2 - 1;
    INF = 9223372036854775807 - (bucketCount);
    PERMANENT_STASH_SIZE = 90;
    stash.preAllocate(PERMANENT_STASH_SIZE * 4);
    printf("Number of leaves:%lld\n", maxOfRandom);
    printf("depth:%lld\n", depth);

    nextDummyCounter = INF;
    blockSize = sizeof (HeapNode); // B  
    printf("block size is:%d\n", blockSize);
    size_t blockCount = (size_t) (Z * bucketCount);
    storeBlockSize = (size_t) (IV + AES::GetCiphertextLength((int) (Z * (blockSize))));
    clen_size = AES::GetCiphertextLength((int) (blockSize) * Z);
    plaintext_size = (blockSize) * Z;
    ocall_setup_heapStore(blockCount, storeBlockSize);
    maxHeightOfAVLTree = (int) floor(log2(blockCount)) + 1;

    unsigned long long first_leaf = bucketCount / 2;

    times.push_back(vector<double>());
    times.push_back(vector<double>());
    times.push_back(vector<double>());
    times.push_back(vector<double>());

    double time;

    unsigned int j = 0;
    HeapBucket* bucket = new HeapBucket();



    int i;
    for (i = 0; i < nodes->size(); i++) {
        (*nodes)[i]->pos = permutation[i];
        (*nodes)[i]->evictionNode = first_leaf + (*nodes)[i]->pos;
    }

    if (beginProfile) {
        ocall_start_timer(10);
    }

    unsigned long long neededDummy = (maxOfRandom * Z);
    for (; i < neededDummy; i++) {
        HeapNode* tmp = new HeapNode();
        tmp->index = i + 1;
        tmp->isDummy = true;
        tmp->pos = permutation[i];
        tmp->evictionNode = permutation[i] + first_leaf;
        nodes->push_back(tmp);
    }

    if (beginProfile) {
        ocall_stop_timer(&time, 10);
        times[0].push_back(time);
    }

    permutation.clear();

    if (beginProfile) {
        ocall_start_timer(10);
    }

    HeapObliviousOperations::bitonicSort(nodes);

    if (beginProfile) {
        ocall_stop_timer(&time, 10);
        times[1].push_back(time);
    }

    vector<long long> indexes;
    vector<HeapBucket> buckets;

    long long first_bucket_of_last_level = bucketCount / 2;

    if (beginProfile) {
        ocall_start_timer(10);
    }


    for (unsigned int i = 0; i < nodes->size(); i++) {
        if (i % 100000 == 0 && i != 0) {
            printf("Creating Buckets:%d/%d\n", i, nodes->size());
        }
        HeapNode* cureNode = (*nodes)[i];
        long long curBucketID = (*nodes)[i]->evictionNode;

        HeapBlock &curBlock = (*bucket).blocks[j];
        curBlock.data.resize(blockSize, 0);
        block tmp = convertNodeToBlock(cureNode);
        curBlock.id = HeapNode::conditional_select((unsigned long long) 0, cureNode->index, cureNode->isDummy);
        for (int k = 0; k < tmp.size(); k++) {
            curBlock.data[k] = HeapNode::conditional_select(curBlock.data[k], tmp[k], cureNode->isDummy);
        }
        delete cureNode;
        j++;

        if (j == Z) {
            indexes.push_back(curBucketID);
            buckets.push_back((*bucket));
            delete bucket;
            bucket = new HeapBucket();
            j = 0;
        }
    }

    //TODO: the update min should be applied to the tree

    for (unsigned int j = 0; j <= indexes.size() / 10000; j++) {
        char* tmp = new char[10000 * storeBlockSize];
        size_t cipherSize = 0;
        for (int i = 0; i < min((int) (indexes.size() - j * 10000), 10000); i++) {
            block b = SerialiseBucket(buckets[j * 10000 + i]);
            block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
            std::memcpy(tmp + i * ciphertext.size(), ciphertext.data(), ciphertext.size());
            cipherSize = ciphertext.size();
        }
        if (min((int) (indexes.size() - j * 10000), 10000) != 0) {
            ocall_nwrite_heapStore(min((int) (indexes.size() - j * 10000), 10000), indexes.data() + j * 10000, (const char*) tmp, cipherSize * min((int) (indexes.size() - j * 10000), 10000));
        }
        delete tmp;
    }

    indexes.clear();
    buckets.clear();

    if (beginProfile) {
        ocall_stop_timer(&time, 10);
        times[3].push_back(time);
        ocall_start_timer(10);
    }

    for (int i = 0; i < first_bucket_of_last_level; i++) {
        if (i % 100000 == 0 && i != 0) {
            printf("Adding Upper Levels Dummy Buckets:%d/%d\n", i, nodes->size());
        }
        for (int z = 0; z < Z; z++) {
            HeapBlock &curBlock = (*bucket).blocks[z];
            curBlock.id = 0;
            curBlock.data.resize(blockSize, 0);
        }
        indexes.push_back(i);
        buckets.push_back((*bucket));
        delete bucket;
        bucket = new HeapBucket();
    }

    if (beginProfile) {
        ocall_stop_timer(&time, 10);
        times[2].push_back(time);
        ocall_start_timer(10);
    }

    delete bucket;

    for (unsigned int j = 0; j <= indexes.size() / 10000; j++) {
        char* tmp = new char[10000 * storeBlockSize];
        size_t cipherSize = 0;
        for (int i = 0; i < min((int) (indexes.size() - j * 10000), 10000); i++) {
            block b = SerialiseBucket(buckets[j * 10000 + i]);
            block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
            std::memcpy(tmp + i * ciphertext.size(), ciphertext.data(), ciphertext.size());
            cipherSize = ciphertext.size();
        }
        if (min((int) (indexes.size() - j * 10000), 10000) != 0) {
            ocall_nwrite_heapStore(min((int) (indexes.size() - j * 10000), 10000), indexes.data() + j * 10000, (const char*) tmp, cipherSize * min((int) (indexes.size() - j * 10000), 10000));
        }
        delete tmp;
    }

    if (beginProfile) {
        ocall_stop_timer(&time, 10);
        times[3][times[3].size() - 1] += time;
    }

    for (int i = 0; i < PERMANENT_STASH_SIZE; i++) {
        HeapNode* tmp = new HeapNode();
        tmp->index = nextDummyCounter;
        tmp->isDummy = true;
        stash.insert(tmp);
    }

}