#include "ORAM.hpp"
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
#include "ObliviousOperations.h"
#include "ORAMEnclaveInterface.h"
#include "Enclave_t.h"  /* print_string */
#include <algorithm>
#include <stdlib.h>
#include <vector>

ORAM::ORAM(long long maxSize, bytes<Key> oram_key, bool simulation)
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
    blockSize = sizeof (Node); // B    
    printf("block size:%d\n", blockSize);
    size_t blockCount = (size_t) (Z * bucketCount);
    storeBlockSize = (size_t) (IV + AES::GetCiphertextLength((int) (Z * (blockSize))));
    clen_size = AES::GetCiphertextLength((int) (blockSize) * Z);
    plaintext_size = (blockSize) * Z;
    if (!simulation) {
        if (useLocalRamStore) {
            localStore = new LocalRAMStore(blockCount, storeBlockSize);
        } else {
            ocall_setup_ramStore(blockCount, storeBlockSize);
        }
    } else {
        ocall_setup_ramStore(depth, -1);
    }

    maxHeightOfAVLTree = (int) floor(log2(blockCount)) + 1;
    times.push_back(vector<double>());
    times.push_back(vector<double>());
    times.push_back(vector<double>());
    times.push_back(vector<double>());
    times.push_back(vector<double>());

    printf("Initializing ORAM Buckets\n");
    Bucket bucket;
    for (int z = 0; z < Z; z++) {
        bucket[z].id = 0;
        bucket[z].data.resize(blockSize, 0);
    }
    if (!simulation) {
        //        InitializeBuckets(0, bucketCount, bucket);
        long long i;
        for (i = 0; i < maxOfRandom - 1; i++) {
            if (i % 10000 == 0) {
                printf("%d/%d\n", i, bucketCount);
            }
            Bucket bucket;
            for (int z = 0; z < Z; z++) {
                bucket[z].id = 0;
                bucket[z].data.resize(blockSize, 0);
            }
            WriteBucket((int) i, bucket);
        }
        for (long long j = 0; i < bucketCount; i++, j++) {
            if (i % 10000 == 0) {
                printf("%d/%d\n", i, bucketCount);
            }
            Bucket bucket;

            bucket[0].id = j;
            Node* tmp = new Node();
            tmp->index = j;
            tmp->pos = j;
            bucket[0].data = convertNodeToBlock(tmp);
            delete tmp;
            for (int z = 1; z < Z; z++) {
                bucket[z].id = 0;
                bucket[z].data.resize(blockSize, 0);
            }
            WriteBucket((long long) i, bucket);
        }
    }
    for (auto i = 0; i < PERMANENT_STASH_SIZE; i++) {
        Node* dummy = new Node();
        dummy->index = nextDummyCounter;
        dummy->evictionNode = -1;
        dummy->isDummy = true;
        stash.insert(dummy);
        nextDummyCounter++;
    }
    printf("End of Initialization\n");
}

ORAM::~ORAM() {
    AES::Cleanup();
}

// Fetches the array index a bucket that lise on a specific path

void ORAM::WriteBucket(long long index, Bucket bucket) {
    block b = SerialiseBucket(bucket);
    block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
    ocall_write_ramStore(index, (const char*) ciphertext.data(), (size_t) ciphertext.size());
}

long long ORAM::GetNodeOnPath(long long leaf, int curDepth) {
    leaf += bucketCount / 2;
    for (int d = depth - 1; d >= 0; d--) {
        bool cond = !Node::CTeq(Node::CTcmp(d, curDepth), -1);
        leaf = Node::conditional_select((leaf + 1) / 2 - 1, leaf, cond);
    }
    return leaf;
}

// Write bucket to a single block

block ORAM::SerialiseBucket(Bucket bucket) {
    block buffer;
    for (int z = 0; z < Z; z++) {
        Block b = bucket[z];
        buffer.insert(buffer.end(), b.data.begin(), b.data.end());
    }
    assert(buffer.size() == Z * (blockSize));
    return buffer;
}

Bucket ORAM::DeserialiseBucket(block buffer) {
    assert(buffer.size() == Z * (blockSize));
    Bucket bucket;
    for (int z = 0; z < Z; z++) {
        Block &curBlock = bucket[z];
        curBlock.data.assign(buffer.begin(), buffer.begin() + blockSize);
        Node* node = convertBlockToNode(curBlock.data);
        bool cond = Node::CTeq(node->index, (unsigned long long) 0);
        node->index = Node::conditional_select(node->index, nextDummyCounter, !cond);
        node->value = Node::conditional_select(node->value, (unsigned long long) 0, !cond);
        node->isDummy = Node::conditional_select(0, 1, !cond);
        stash.insert(node);
        buffer.erase(buffer.begin(), buffer.begin() + blockSize);
    }

    return bucket;
}

vector<Bucket> ORAM::ReadBuckets(vector<long long> indexes) {
    vector<Bucket> res;
    if (indexes.size() == 0) {
        return res;
    }
    if (useLocalRamStore) {
        for (unsigned int i = 0; i < indexes.size(); i++) {
            block ciphertext = localStore->Read(indexes[i]);
            block buffer = AES::Decrypt(key, ciphertext, clen_size);
            Bucket bucket = DeserialiseBucket(buffer);
            res.push_back(bucket);
        }
    } else {
        size_t readSize;
        char* tmp = new char[indexes.size() * storeBlockSize];
        ocall_nread_ramStore(&readSize, indexes.size(), indexes.data(), tmp, indexes.size() * storeBlockSize);
        for (unsigned int i = 0; i < indexes.size(); i++) {
            block ciphertext(tmp + i*readSize, tmp + (i + 1) * readSize);
            block buffer = AES::Decrypt(key, ciphertext, clen_size);
            Bucket bucket = DeserialiseBucket(buffer);
            res.push_back(bucket);
        }
        delete tmp;
    }
    return res;
}

void ORAM::InitializeBuckets(long long strtindex, long long endindex, Bucket bucket) {
    block b = SerialiseBucket(bucket);
    block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
    if (useLocalRamStore) {
        for (long long i = strtindex; i < endindex; i++) {
            localStore->Write(i, ciphertext);
        }
    } else {
        ocall_initialize_ramStore(strtindex, endindex, (const char*) ciphertext.data(), (size_t) ciphertext.size());
    }
}

void ORAM::WriteBuckets(vector<long long> indexes, vector<Bucket> buckets) {
    for (unsigned int i = 0; i < indexes.size(); i++) {
        if (virtualStorage.count(indexes[i]) != 0) {
            virtualStorage.erase(indexes[i]);
        }
        virtualStorage[indexes[i]] = buckets[i];
    }
}

void ORAM::EvictBuckets() {
    vector<long long> indexes;
    vector<Bucket> buckets;
    for (auto item : virtualStorage) {
        indexes.push_back(item.first);
        buckets.push_back(item.second);
    }
    if (useLocalRamStore) {
        for (unsigned int i = 0; i < indexes.size(); i++) {
            block b = SerialiseBucket(buckets[i]);
            block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
            localStore->Write(indexes[i], ciphertext);
        }
    } else {
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
                ocall_nwrite_ramStore(min((int) (indexes.size() - j * 10000), 10000), indexes.data() + j * 10000, (const char*) tmp, cipherSize * min((int) (indexes.size() - j * 10000), 10000));
            }
            delete tmp;
        }
    }
    virtualStorage.clear();
}
// Fetches blocks along a path, adding them to the stash

void ORAM::FetchPath(long long leaf) {
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
        Bucket bucket = virtualStorage[existingIndexes[i]];
        for (int z = 0; z < Z; z++) {
            Block &curBlock = bucket[z];
            Node* node = convertBlockToNode(curBlock.data);
            bool cond = Node::CTeq(node->index, (unsigned long long) 0);
            node->index = Node::conditional_select(node->index, nextDummyCounter, !cond);
            node->isDummy = Node::conditional_select(0, 1, !cond);
            stash.insert(node);
        }
    }
}

// Gets the data of a block in the stash

unsigned long long ORAM::Access(unsigned long long bid, unsigned long long pos, unsigned long long& content) {
    if (bid == 0) {
        throw runtime_error("Node id is not set");
    }
    double time;
    if (beginProfile) {
        ocall_start_timer(10);
    }

    FetchPath(pos);

    if (beginProfile) {
        ocall_stop_timer(&time, 10);
        times[0].push_back(time);
    }

    unsigned long long newPos = RandomPath();
    bool write = !Node::CTeq(content, (unsigned long long) 0);
    for (Node* node : stash.nodes) {
        bool choice = Node::CTeq(0, Node::CTcmp(node->index, bid));
        content = Node::conditional_select(node->value, content, choice && !write);
        node->value = Node::conditional_select(content, node->value, choice && write);
        node->pos = Node::conditional_select(newPos, node->pos, choice);
    }
    currentLeaf = pos;
    evict(true);
    return newPos;
}

Node* ORAM::convertBlockToNode(block b) {
    Node* node = new Node();
    std::array<byte_t, sizeof (Node) > arr;
    std::copy(b.begin(), b.begin() + sizeof (Node), arr.begin());
    from_bytes(arr, *node);
    return node;
}

block ORAM::convertNodeToBlock(Node* node) {
    std::array<byte_t, sizeof (Node) > data = to_bytes(*node);
    block b(data.begin(), data.end());
    return b;
}

void ORAM::evict(bool evictBuckets) {
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


    for (Node* node : stash.nodes) {
        long long xorVal = 0;
        xorVal = Node::conditional_select((unsigned long long) 0, node->pos ^ currentLeaf, node->isDummy);
        long long indx = 0;

        indx = (long long) floor(log2(Node::conditional_select(xorVal, (long long) 1, Node::CTcmp(xorVal, 0))));
        indx = indx + Node::conditional_select(1, 0, Node::CTcmp(xorVal, 0));

        for (long long i = 0; i < firstIndexes.size(); i++) {
            bool choice = Node::CTeq(i, indx);
            long long value = firstIndexes[i];
            node->evictionNode = Node::conditional_select(firstIndexes[indx], node->evictionNode, !node->isDummy && choice);
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
            Node* dummy = new Node();
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
        printf("Creating Dummy Blocks for each Bucket:%f\n", time);
        ocall_start_timer(10);
    }

    if (beginProfile) {
        ocall_start_timer(10);
    }

    ObliviousOperations::oblixmergesort(&stash.nodes);

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
        Node* curNode = stash.nodes[i];
        bool firstCond = (!Node::CTeq(Node::CTcmp(counter - (depth - level) * Z, Z), -1));
        bool secondCond = Node::CTeq(Node::CTcmp(curNode->evictionNode, currentID), 0);
        bool thirdCond = (!Node::CTeq(Node::CTcmp(counter, Z * depth), -1)) || curNode->isDummy;
        bool fourthCond = Node::CTeq(curNode->evictionNode, currentID);

        long long tmpEvictionNode = GetNodeOnPath(currentLeaf, depth - (int) floor(counter / Z));
        long long tmpcurrentID = GetNodeOnPath(currentLeaf, level - 1);
        curNode->evictionNode = Node::conditional_select((long long) - 1, curNode->evictionNode, firstCond && secondCond && thirdCond);
        curNode->evictionNode = Node::conditional_select(tmpEvictionNode, curNode->evictionNode, firstCond && secondCond && !thirdCond);
        counter = Node::conditional_select(counter + 1, counter, firstCond && secondCond && !thirdCond);
        counter = Node::conditional_select(counter + 1, counter, !firstCond && fourthCond);
        level = Node::conditional_select(level - 1, level, firstCond && !secondCond);
        i = Node::conditional_select(i - 1, i, firstCond && !secondCond);
        currentID = Node::conditional_select(tmpcurrentID, currentID, firstCond&&!secondCond);

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

    //    ObliviousOperations::compaction(&stash.nodes);
    ObliviousOperations::oblixmergesort(&stash.nodes);

    if (profile) {
        ocall_stop_timer(&time, 10);
        printf("Oblivious Compaction: %f\n", time);
        ocall_start_timer(10);
    }

    unsigned int j = 0;
    Bucket* bucket = new Bucket();
    for (int i = 0; i < (depth + 1) * Z; i++) {
        Node* cureNode = stash.nodes[i];
        long long curBucketID = cureNode->evictionNode;
        Block &curBlock = (*bucket)[j];
        curBlock.data.resize(blockSize, 0);
        block tmp = convertNodeToBlock(cureNode);
        curBlock.id = Node::conditional_select((unsigned long long) 0, cureNode->index, cureNode->isDummy);
        for (int k = 0; k < tmp.size(); k++) {
            curBlock.data[k] = Node::conditional_select(curBlock.data[k], tmp[k], cureNode->isDummy);
        }
        delete cureNode;
        j++;

        if (j == Z) {
            virtualStorage[curBucketID] = (*bucket);
            delete bucket;
            bucket = new Bucket();
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

    EvictBuckets();

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

void ORAM::start(bool isBatchWrite) {
    this->batchWrite = isBatchWrite;
    readCnt = 0;
}

void ORAM::prepareForEvictionTest() {
    long long leaf = 10;
    currentLeaf = leaf;
    Node* nd = new Node();
    nd->isDummy = false;
    nd->evictionNode = GetNodeOnPath(leaf, depth);
    nd->index = 1;
    nextDummyCounter++;
    nd->pos = leaf;
    stash.insert(nd);
    for (int i = 0; i <= Z * depth; i++) {
        Node* n = new Node();
        n->isDummy = true;
        n->evictionNode = GetNodeOnPath(leaf, depth);
        n->index = nextDummyCounter;
        nextDummyCounter++;
        n->pos = leaf;
        stash.insert(n);
    }
}

unsigned long long ORAM::RandomPath() {
    uint32_t val;
    sgx_read_rand((unsigned char *) &val, 4);
    return val % (maxOfRandom);
}

ORAM::ORAM(long long maxSize, bytes<Key> oram_key, vector<Node*>* nodes, map<unsigned long long, unsigned long long> permutation)
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
    blockSize = sizeof (Node); // B  
    printf("block size is:%d\n", blockSize);
    size_t blockCount = (size_t) (Z * bucketCount);
    storeBlockSize = (size_t) (IV + AES::GetCiphertextLength((int) (Z * (blockSize))));
    clen_size = AES::GetCiphertextLength((int) (blockSize) * Z);
    plaintext_size = (blockSize) * Z;
    ocall_setup_ramStore(blockCount, storeBlockSize);
    maxHeightOfAVLTree = (int) floor(log2(blockCount)) + 1;

    unsigned long long first_leaf = bucketCount / 2;

    times.push_back(vector<double>());
    times.push_back(vector<double>());
    times.push_back(vector<double>());
    times.push_back(vector<double>());

    double time;

    unsigned int j = 0;
    Bucket* bucket = new Bucket();



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
        Node* tmp = new Node();
        tmp->index = i + 1;
        tmp->value = i + 1;
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

    ObliviousOperations::bitonicSort(nodes);

    if (beginProfile) {
        ocall_stop_timer(&time, 10);
        times[1].push_back(time);
    }

    vector<long long> indexes;
    vector<Bucket> buckets;

    long long first_bucket_of_last_level = bucketCount / 2;

    if (beginProfile) {
        ocall_start_timer(10);
    }


    for (unsigned int i = 0; i < nodes->size(); i++) {
        if (i % 100000 == 0 && i != 0) {
            printf("Creating Buckets:%d/%d\n", i, nodes->size());
        }
        Node* cureNode = (*nodes)[i];
        long long curBucketID = (*nodes)[i]->evictionNode;

        Block &curBlock = (*bucket)[j];
        curBlock.data.resize(blockSize, 0);
        block tmp = convertNodeToBlock(cureNode);
        curBlock.id = Node::conditional_select((unsigned long long) 0, cureNode->index, cureNode->isDummy);
        for (int k = 0; k < tmp.size(); k++) {
            curBlock.data[k] = Node::conditional_select(curBlock.data[k], tmp[k], cureNode->isDummy);
        }
        delete cureNode;
        j++;

        if (j == Z) {
            indexes.push_back(curBucketID);
            buckets.push_back((*bucket));
            delete bucket;
            bucket = new Bucket();
            j = 0;
        }
    }


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
            ocall_nwrite_ramStore(min((int) (indexes.size() - j * 10000), 10000), indexes.data() + j * 10000, (const char*) tmp, cipherSize * min((int) (indexes.size() - j * 10000), 10000));
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
            Block &curBlock = (*bucket)[z];
            curBlock.id = 0;
            curBlock.data.resize(blockSize, 0);
        }
        indexes.push_back(i);
        buckets.push_back((*bucket));
        delete bucket;
        bucket = new Bucket();
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
            ocall_nwrite_ramStore(min((int) (indexes.size() - j * 10000), 10000), indexes.data() + j * 10000, (const char*) tmp, cipherSize * min((int) (indexes.size() - j * 10000), 10000));
        }
        delete tmp;
    }

    if (beginProfile) {
        ocall_stop_timer(&time, 10);
        times[3][times[3].size() - 1] += time;
    }

    for (int i = 0; i < PERMANENT_STASH_SIZE; i++) {
        Node* tmp = new Node();
        tmp->index = nextDummyCounter;
        tmp->isDummy = true;
        stash.insert(tmp);
    }

}