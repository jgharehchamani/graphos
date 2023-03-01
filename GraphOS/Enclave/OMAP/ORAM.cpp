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

void check_memory3(string text) {
    unsigned int required = 0x4f00000; // adapt to native uint
    char *mem = NULL;
    while (mem == NULL) {
        mem = (char*) malloc(required);
        if ((required -= 8) < 0xFFF) {
            if (mem) free(mem);
            printf("Cannot allocate enough memory\n");
            return;
        }
    }

    free(mem);
    mem = (char*) malloc(required);
    if (mem == NULL) {
        printf("Cannot enough allocate memory\n");
        return;
    }
    printf("%s = %d\n", text.c_str(), required);
    free(mem);
}

ORAM::ORAM(long long maxSize, bytes<Key> oram_key, bool simulation, bool isEmptyMap)
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

    printf("Initializing ORAM Buckets\n");
    Bucket bucket;
    for (int z = 0; z < Z; z++) {
        bucket[z].id = 0;
        bucket[z].data.resize(blockSize, 0);
    }
    if (!simulation && isEmptyMap) {
        InitializeORAMBuckets();
    }
    for (auto i = 0; i < PERMANENT_STASH_SIZE; i++) {
        Node* dummy = new Node();
        dummy->index = nextDummyCounter;
        dummy->evictionNode = -1;
        dummy->isDummy = true;
        dummy->modified = false;
        dummy->leftID = 0;
        dummy->leftPos = 0;
        dummy->rightPos = 0;
        dummy->rightID = 0;
        dummy->pos = 0;
        dummy->height = 1;
        stash.insert(dummy);
        nextDummyCounter++;
    }
    printf("End of Initialization\n");
}

ORAM::~ORAM() {
    AES::Cleanup();
}

void ORAM::InitializeBucketsOneByOne() {
    for (long long i = 0; i < bucketCount; i++) {
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
}

void ORAM::InitializeBucketsInBatch() {
    int batchSize = 10000;

    for (unsigned int j = 0; j <= bucketCount / batchSize; j++) {
        if (j % 10 == 0) {
            printf("%d/%d\n", j, bucketCount / batchSize);
        }
        char* tmp = new char[batchSize * storeBlockSize];
        vector<long long> indexes;
        size_t cipherSize = 0;
        for (int i = 0; i < min((int) (bucketCount - j * batchSize), batchSize); i++) {
            Bucket bucket;
            for (int z = 0; z < Z; z++) {
                bucket[z].id = 0;
                bucket[z].data.resize(blockSize, 0);
            }
            block b = SerialiseBucket(bucket);
            block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
            indexes.push_back(j * batchSize + i);
            std::memcpy(tmp + i * ciphertext.size(), ciphertext.data(), ciphertext.size());
            cipherSize = ciphertext.size();
        }
        if (min((int) (bucketCount - j * batchSize), batchSize) != 0) {
            ocall_nwrite_ramStore(min((int) (bucketCount - j * batchSize), batchSize), indexes.data(), (const char*) tmp, cipherSize * min((int) (bucketCount - j * batchSize), batchSize));
        }
        delete tmp;
        indexes.clear();
    }
}

void ORAM::InitializeORAMBuckets() {
    double time;
    ocall_start_timer(687);

    //    InitializeBuckets(0, bucketCount, bucket);
    //    InitializeBucketsOneByOne();
    InitializeBucketsInBatch();


    ocall_stop_timer(&time, 687);
    printf("ORAM Initialization Time:%f\n", time);
}

void ORAM::WriteBucket(long long index, Bucket bucket) {
    block b = SerialiseBucket(bucket);
    block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
    ocall_write_ramStore(index, (const char*) ciphertext.data(), (size_t) ciphertext.size());
}
// Fetches the array index a bucket that lise on a specific path

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
        node->isDummy = Node::conditional_select(0, 1, !cond);
        node->pos = Node::conditional_select(RandomPath(), node->pos, cond);
        if (isIncomepleteRead) {
            incStash.insert(node);
        } else {
            stash.insert(node);
        }
        buffer.erase(buffer.begin(), buffer.begin() + blockSize);
    }
    return bucket;
}

void ORAM::ReadBuckets(vector<long long> indexes) {
    if (indexes.size() == 0) {
        return;
    }
    if (useLocalRamStore) {
        for (unsigned int i = 0; i < indexes.size(); i++) {
            block ciphertext = localStore->Read(indexes[i]);
            block buffer = AES::Decrypt(key, ciphertext, clen_size);
            Bucket bucket = DeserialiseBucket(buffer);
        }
    } else {
        size_t readSize;
        char* tmp = new char[indexes.size() * storeBlockSize];
        ocall_nread_ramStore(&readSize, indexes.size(), indexes.data(), tmp, indexes.size() * storeBlockSize);
        for (unsigned int i = 0; i < indexes.size(); i++) {
            block ciphertext(tmp + i*readSize, tmp + (i + 1) * readSize);
            block buffer = AES::Decrypt(key, ciphertext, clen_size);
            Bucket bucket = DeserialiseBucket(buffer);
            if (!evictBuckets) {
                virtualStorage[indexes[i]] = bucket;
            }
        }
        delete[] tmp;
    }
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
    if (!shutdownEvictBucket) {
        unordered_map<long long, Bucket>::iterator it = virtualStorage.begin();
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
                    ocall_nwrite_ramStore(min((int) (virtualStorage.size() - j * 10000), 10000), indexes.data(), (const char*) tmp, cipherSize * min((int) (virtualStorage.size() - j * 10000), 10000));
                }
                delete tmp;
                indexes.clear();
            }
        }
        virtualStorage.clear();
    }
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
            if (isIncomepleteRead) {
                incStash.insert(node);
            } else {
                stash.insert(node);
            }
        }
    }

}

//    for (int i = 0; i < stash.nodes.size(); i++) {
//        if (stash.nodes[i]->isDummy == false)
//            printf("1.index:%llu key:%lld  pos:%llu, isDummy:%d leftKey:%lld left Pos:%llu right key:%lld right pos:%llu\n", stash.nodes[i]->index, stash.nodes[i]->key.getValue(), stash.nodes[i]->pos, stash.nodes[i]->isDummy ? 1 : 0, stash.nodes[i]->leftID.getValue(), stash.nodes[i]->leftPos, stash.nodes[i]->rightID.getValue(), stash.nodes[i]->rightPos);
//    }

Node* ORAM::ReadWrite(Bid bid, Node* inputnode, unsigned long long lastLeaf, unsigned long long newLeaf, bool isRead, bool isDummy, bool isIncRead) {
    if (bid == 0) {
        printf("bid is 0 dummy is:%d\n", isDummy ? 1 : 0);
        throw runtime_error("Node id is not set");
    }
    accessCounter++;

    isIncomepleteRead = isIncRead;

    unsigned long long newPos = RandomPath();
    unsigned long long fetchPos = Node::conditional_select(newPos, lastLeaf, isDummy);

    inputnode->pos = fetchPos;

    FetchPath(fetchPos);


    if (!isIncomepleteRead) {
        currentLeaf = fetchPos;
    }

    Node* tmpWrite = Node::clone(inputnode);
    tmpWrite->pos = newLeaf;

    Node* res = new Node();
    res->isDummy = true;
    res->index = nextDummyCounter++;
    res->key = nextDummyCounter++;
    bool write = !isRead;

    vector<Node*> nodesList(stash.nodes.begin(), stash.nodes.end());

    if (isIncomepleteRead) {
        nodesList.insert(nodesList.end(), incStash.nodes.begin(), incStash.nodes.end());
    }

    for (Node* node : nodesList) {
        bool match = Node::CTeq(Bid::CTcmp(node->key, bid), 0) && !node->isDummy;
        node->isDummy = Node::conditional_select(true, node->isDummy, !isDummy && match && write);
        node->pos = Node::conditional_select(newLeaf, node->pos, !isDummy && match);
        bool choice = !isDummy && match && isRead && !node->isDummy;
        res->index = Node::conditional_select((long long) node->index, (long long) res->index, choice);
        res->isDummy = Node::conditional_select(node->isDummy, res->isDummy, choice);
        res->pos = Node::conditional_select((long long) node->pos, (long long) res->pos, choice);
        for (int k = 0; k < res->value.size(); k++) {
            res->value[k] = Node::conditional_select(node->value[k], res->value[k], choice);
        }
        for (int k = 0; k < res->dum.size(); k++) {
            res->dum[k] = Node::conditional_select(node->dum[k], res->dum[k], choice);
        }
        res->evictionNode = Node::conditional_select(node->evictionNode, res->evictionNode, choice);
        res->height = Node::conditional_select(node->height, res->height, choice);
        res->leftPos = Node::conditional_select(node->leftPos, res->leftPos, choice);
        res->rightPos = Node::conditional_select(node->rightPos, res->rightPos, choice);
        for (int k = 0; k < res->key.id.size(); k++) {
            res->key.id[k] = Node::conditional_select(node->key.id[k], res->key.id[k], choice);
        }
        for (int k = 0; k < res->leftID.id.size(); k++) {
            res->leftID.id[k] = Node::conditional_select(node->leftID.id[k], res->leftID.id[k], choice);
        }
        for (int k = 0; k < res->rightID.id.size(); k++) {
            res->rightID.id[k] = Node::conditional_select(node->rightID.id[k], res->rightID.id[k], choice);
        }
    }

    if (!isIncomepleteRead) {
        stash.insert(tmpWrite);
    } else {
        incStash.insert(tmpWrite);
    }

    if (!isIncomepleteRead) {
        evict(evictBuckets);
    } else {
        for (Node* item : incStash.nodes) {
            delete item;
        }
        incStash.nodes.clear();
    }

    isIncomepleteRead = false;

    return res;
}

Node* ORAM::ReadWriteTest(Bid bid, Node* inputnode, unsigned long long lastLeaf, unsigned long long newLeaf, bool isRead, bool isDummy, bool isIncRead) {
    if (bid == 0) {
        printf("bid is 0 dummy is:%d\n", isDummy ? 1 : 0);
        throw runtime_error("Node id is not set");
    }
    accessCounter++;

    isIncomepleteRead = isIncRead;

    unsigned long long newPos;
    unsigned long long fetchPos = Node::conditional_select(newPos, lastLeaf, isDummy);

    inputnode->pos = fetchPos;

    FetchPath(fetchPos);


    if (!isIncomepleteRead) {
        currentLeaf = fetchPos;
    }

    Node* tmpWrite = Node::clone(inputnode);
    tmpWrite->pos = newLeaf;

    Node* res = new Node();
    res->isDummy = true;
    res->index = nextDummyCounter++;
    res->key = nextDummyCounter++;
    bool write = !isRead;

    vector<Node*> nodesList(stash.nodes.begin(), stash.nodes.end());

    if (isIncomepleteRead) {
        nodesList.insert(nodesList.end(), incStash.nodes.begin(), incStash.nodes.end());
    }

    for (Node* node : nodesList) {
        bool match = Node::CTeq(Bid::CTcmp(node->key, bid), 0) && !node->isDummy;
        node->isDummy = Node::conditional_select(true, node->isDummy, !isDummy && match && write);
        node->pos = Node::conditional_select(newLeaf, node->pos, !isDummy && match);
        bool choice = !isDummy && match && isRead && !node->isDummy;
        res->index = Node::conditional_select((long long) node->index, (long long) res->index, choice);
        res->isDummy = Node::conditional_select(node->isDummy, res->isDummy, choice);
        res->pos = Node::conditional_select((long long) node->pos, (long long) res->pos, choice);
        for (int k = 0; k < res->value.size(); k++) {
            res->value[k] = Node::conditional_select(node->value[k], res->value[k], choice);
        }
        for (int k = 0; k < res->dum.size(); k++) {
            res->dum[k] = Node::conditional_select(node->dum[k], res->dum[k], choice);
        }
        res->evictionNode = Node::conditional_select(node->evictionNode, res->evictionNode, choice);
        res->height = Node::conditional_select(node->height, res->height, choice);
        res->leftPos = Node::conditional_select(node->leftPos, res->leftPos, choice);
        res->rightPos = Node::conditional_select(node->rightPos, res->rightPos, choice);
        for (int k = 0; k < res->key.id.size(); k++) {
            res->key.id[k] = Node::conditional_select(node->key.id[k], res->key.id[k], choice);
        }
        for (int k = 0; k < res->leftID.id.size(); k++) {
            res->leftID.id[k] = Node::conditional_select(node->leftID.id[k], res->leftID.id[k], choice);
        }
        for (int k = 0; k < res->rightID.id.size(); k++) {
            res->rightID.id[k] = Node::conditional_select(node->rightID.id[k], res->rightID.id[k], choice);
        }
    }

    if (!isIncomepleteRead) {
        stash.insert(tmpWrite);
    } else {
        incStash.insert(tmpWrite);
    }

    if (!isIncomepleteRead) {
        evict(evictBuckets);
    } else {
        for (Node* item : incStash.nodes) {
            delete item;
        }
        incStash.nodes.clear();
    }

    isIncomepleteRead = false;

    return res;
}

Node* ORAM::ReadWrite(Bid bid, unsigned long long lastLeaf, unsigned long long newLeaf, bool isDummy, unsigned long long newChildPos, Bid targetNode) {
    if (bid == 0) {
        printf("bid is 0 dummy is:%d\n", isDummy ? 1 : 0);
        throw runtime_error("Node id is not set");
    }
    accessCounter++;


    unsigned long long newPos = RandomPath();
    unsigned long long fetchPos = Node::conditional_select(newPos, lastLeaf, isDummy);

    FetchPath(fetchPos);


    currentLeaf = fetchPos;

    Node* res = new Node();
    res->isDummy = true;
    res->index = nextDummyCounter++;
    res->key = nextDummyCounter++;

    for (Node* node : stash.nodes) {
        bool match = Node::CTeq(Bid::CTcmp(node->key, bid), 0) && !node->isDummy;
        node->pos = Node::conditional_select(newLeaf, node->pos, !isDummy && match);

        bool choice = !isDummy && match && !node->isDummy;

        bool leftChild = Node::CTeq(Bid::CTcmp(node->key, targetNode), 1);
        bool rightChild = Node::CTeq(Bid::CTcmp(node->key, targetNode), -1);

        res->index = Node::conditional_select((long long) node->index, (long long) res->index, choice);
        res->isDummy = Node::conditional_select(node->isDummy, res->isDummy, choice);
        res->pos = Node::conditional_select((long long) node->pos, (long long) res->pos, choice);
        for (int k = 0; k < res->value.size(); k++) {
            res->value[k] = Node::conditional_select(node->value[k], res->value[k], choice);
        }
        for (int k = 0; k < res->dum.size(); k++) {
            res->dum[k] = Node::conditional_select(node->dum[k], res->dum[k], choice);
        }
        res->evictionNode = Node::conditional_select(node->evictionNode, res->evictionNode, choice);
        res->height = Node::conditional_select(node->height, res->height, choice);
        res->leftPos = Node::conditional_select(node->leftPos, res->leftPos, choice);
        res->rightPos = Node::conditional_select(node->rightPos, res->rightPos, choice);
        for (int k = 0; k < res->key.id.size(); k++) {
            res->key.id[k] = Node::conditional_select(node->key.id[k], res->key.id[k], choice);
        }
        for (int k = 0; k < res->leftID.id.size(); k++) {
            res->leftID.id[k] = Node::conditional_select(node->leftID.id[k], res->leftID.id[k], choice);
        }
        for (int k = 0; k < res->rightID.id.size(); k++) {
            res->rightID.id[k] = Node::conditional_select(node->rightID.id[k], res->rightID.id[k], choice);
        }

        //these 2 should be after result set(here is correct)
        node->leftPos = Node::conditional_select(newChildPos, node->leftPos, !isDummy && match && leftChild);
        node->rightPos = Node::conditional_select(newChildPos, node->rightPos, !isDummy && match && rightChild);

    }

    evict(evictBuckets);
    return res;
}

Node* ORAM::ReadWrite(Bid bid, unsigned long long lastLeaf, unsigned long long newLeaf, bool isDummy, unsigned long long newChildPos, Bid targetNode, std::array< byte_t, 16> newVec) {
    if (bid == 0) {
        printf("bid is 0 dummy is:%d\n", isDummy ? 1 : 0);
        throw runtime_error("Node id is not set");
    }
    accessCounter++;


    unsigned long long newPos = RandomPath();
    unsigned long long fetchPos = Node::conditional_select(newPos, lastLeaf, isDummy);

    FetchPath(fetchPos);


    currentLeaf = fetchPos;

    Node* res = new Node();
    res->isDummy = true;
    res->index = nextDummyCounter++;
    res->key = nextDummyCounter++;

    for (Node* node : stash.nodes) {
        bool match = Node::CTeq(Bid::CTcmp(node->key, bid), 0) && !node->isDummy;
        node->pos = Node::conditional_select(newLeaf, node->pos, !isDummy && match);

        bool choice = !isDummy && match && !node->isDummy;

        bool leftChild = Node::CTeq(Bid::CTcmp(node->key, targetNode), 1);
        bool rightChild = Node::CTeq(Bid::CTcmp(node->key, targetNode), -1);
        bool curNodeIsTarget = Node::CTeq(Bid::CTcmp(node->key, targetNode), 0);

        res->index = Node::conditional_select((long long) node->index, (long long) res->index, choice);
        res->isDummy = Node::conditional_select(node->isDummy, res->isDummy, choice);
        res->pos = Node::conditional_select((long long) node->pos, (long long) res->pos, choice);
        for (int k = 0; k < res->value.size(); k++) {
            res->value[k] = Node::conditional_select(node->value[k], res->value[k], choice);
        }
        for (int k = 0; k < res->dum.size(); k++) {
            res->dum[k] = Node::conditional_select(node->dum[k], res->dum[k], choice);
        }
        res->evictionNode = Node::conditional_select(node->evictionNode, res->evictionNode, choice);
        res->height = Node::conditional_select(node->height, res->height, choice);
        res->leftPos = Node::conditional_select(node->leftPos, res->leftPos, choice);
        res->rightPos = Node::conditional_select(node->rightPos, res->rightPos, choice);
        for (int k = 0; k < res->key.id.size(); k++) {
            res->key.id[k] = Node::conditional_select(node->key.id[k], res->key.id[k], choice);
        }
        for (int k = 0; k < res->leftID.id.size(); k++) {
            res->leftID.id[k] = Node::conditional_select(node->leftID.id[k], res->leftID.id[k], choice);
        }
        for (int k = 0; k < res->rightID.id.size(); k++) {
            res->rightID.id[k] = Node::conditional_select(node->rightID.id[k], res->rightID.id[k], choice);
        }

        //these 2 should be after result set(here is correct)
        node->leftPos = Node::conditional_select(newChildPos, node->leftPos, !isDummy && match && leftChild);
        node->rightPos = Node::conditional_select(newChildPos, node->rightPos, !isDummy && match && rightChild);
        for (int k = 0; k < node->value.size(); k++) {
            node->value[k] = Node::conditional_select(newVec[k], node->value[k], curNodeIsTarget && match);
        }

    }

    evict(evictBuckets);
    return res;
}

vector<string> ORAM::split(const string& str, const string& delim) {
    vector<string> tokens = {"", ""};
    int pos = 0;
    for (int i = 0; i < str.length(); i++) {
        bool cond = Node::CTeq(str.at(i), '-');
        pos = Node::conditional_select(i, pos, cond);
    }
    string token = str.substr(0, pos);
    tokens[0] = token;
    int begin = Node::conditional_select(pos, pos + 1, Node::CTeq(Node::CTcmp(pos + 1, str.length()), 1));
    token = str.substr(begin, str.length());
    tokens[1] = token;
    return tokens;
}

Node* ORAM::ReadWrite(Bid bid, unsigned long long lastLeaf, unsigned long long newLeaf, bool isDummy, unsigned long long newChildPos, Bid targetNode, string part, bool isFirstPart) {
    if (bid == 0) {
        printf("bid is 0 dummy is:%d\n", isDummy ? 1 : 0);
        throw runtime_error("Node id is not set");
    }
    accessCounter++;


    unsigned long long newPos = RandomPath();
    unsigned long long fetchPos = Node::conditional_select(newPos, lastLeaf, isDummy);

    FetchPath(fetchPos);


    currentLeaf = fetchPos;

    Node* res = new Node();
    res->isDummy = true;
    res->index = nextDummyCounter++;
    res->key = nextDummyCounter++;

    for (Node* node : stash.nodes) {
        bool match = Node::CTeq(Bid::CTcmp(node->key, bid), 0) && !node->isDummy;
        node->pos = Node::conditional_select(newLeaf, node->pos, !isDummy && match);

        bool choice = !isDummy && match && !node->isDummy;

        bool leftChild = Node::CTeq(Bid::CTcmp(node->key, targetNode), 1);
        bool rightChild = Node::CTeq(Bid::CTcmp(node->key, targetNode), -1);
        bool curNodeIsTarget = Node::CTeq(Bid::CTcmp(node->key, targetNode), 0);

        res->index = Node::conditional_select((long long) node->index, (long long) res->index, choice);
        res->isDummy = Node::conditional_select(node->isDummy, res->isDummy, choice);
        res->pos = Node::conditional_select((long long) node->pos, (long long) res->pos, choice);
        for (int k = 0; k < res->value.size(); k++) {
            res->value[k] = Node::conditional_select(node->value[k], res->value[k], choice);
        }
        for (int k = 0; k < res->dum.size(); k++) {
            res->dum[k] = Node::conditional_select(node->dum[k], res->dum[k], choice);
        }
        res->evictionNode = Node::conditional_select(node->evictionNode, res->evictionNode, choice);
        res->height = Node::conditional_select(node->height, res->height, choice);
        res->leftPos = Node::conditional_select(node->leftPos, res->leftPos, choice);
        res->rightPos = Node::conditional_select(node->rightPos, res->rightPos, choice);
        for (int k = 0; k < res->key.id.size(); k++) {
            res->key.id[k] = Node::conditional_select(node->key.id[k], res->key.id[k], choice);
        }
        for (int k = 0; k < res->leftID.id.size(); k++) {
            res->leftID.id[k] = Node::conditional_select(node->leftID.id[k], res->leftID.id[k], choice);
        }
        for (int k = 0; k < res->rightID.id.size(); k++) {
            res->rightID.id[k] = Node::conditional_select(node->rightID.id[k], res->rightID.id[k], choice);
        }

        //these 2 should be after result set(here is correct)
        node->leftPos = Node::conditional_select(newChildPos, node->leftPos, !isDummy && match && leftChild);
        node->rightPos = Node::conditional_select(newChildPos, node->rightPos, !isDummy && match && rightChild);

        string tmpRes = "0-0             ";
        for (int i = 0; i < node->value.size(); i++) {
            tmpRes[i] = Node::conditional_select(node->value[i], tmpRes[i], curNodeIsTarget);
        }
        vector<string> parts = split(tmpRes, "-");
        string newval = "";
        if (isFirstPart) {
            newval = part + "-" + parts[1];
        } else {
            newval = parts[0] + "-" + part;
        }
        std::array< byte_t, 16> newVec;
        std::fill(newVec.begin(), newVec.end(), 0);
        std::copy(newval.begin(), newval.end(), newVec.begin());

        for (int k = 0; k < node->value.size(); k++) {
            node->value[k] = Node::conditional_select(newVec[k], node->value[k], curNodeIsTarget && match);
        }

    }

    evict(evictBuckets);
    return res;
}

Node* ORAM::ReadWrite(Bid bid, unsigned long long lastLeaf, unsigned long long newLeaf, bool isDummy, unsigned long long newChildPos, Bid targetNode, bool isFirstPart) {
    if (bid == 0) {
        printf("bid is 0 dummy is:%d\n", isDummy ? 1 : 0);
        throw runtime_error("Node id is not set");
    }
    accessCounter++;


    unsigned long long newPos = RandomPath();
    unsigned long long fetchPos = Node::conditional_select(newPos, lastLeaf, isDummy);

    FetchPath(fetchPos);


    currentLeaf = fetchPos;

    Node* res = new Node();
    res->isDummy = true;
    res->index = nextDummyCounter++;
    res->key = nextDummyCounter++;

    for (Node* node : stash.nodes) {
        bool match = Node::CTeq(Bid::CTcmp(node->key, bid), 0) && !node->isDummy;
        node->pos = Node::conditional_select(newLeaf, node->pos, !isDummy && match);

        bool choice = !isDummy && match && !node->isDummy;

        bool leftChild = Node::CTeq(Bid::CTcmp(node->key, targetNode), 1);
        bool rightChild = Node::CTeq(Bid::CTcmp(node->key, targetNode), -1);
        bool curNodeIsTarget = Node::CTeq(Bid::CTcmp(node->key, targetNode), 0);

        res->index = Node::conditional_select((long long) node->index, (long long) res->index, choice);
        res->isDummy = Node::conditional_select(node->isDummy, res->isDummy, choice);
        res->pos = Node::conditional_select((long long) node->pos, (long long) res->pos, choice);
        for (int k = 0; k < res->value.size(); k++) {
            res->value[k] = Node::conditional_select(node->value[k], res->value[k], choice);
        }
        for (int k = 0; k < res->dum.size(); k++) {
            res->dum[k] = Node::conditional_select(node->dum[k], res->dum[k], choice);
        }
        res->evictionNode = Node::conditional_select(node->evictionNode, res->evictionNode, choice);
        res->height = Node::conditional_select(node->height, res->height, choice);
        res->leftPos = Node::conditional_select(node->leftPos, res->leftPos, choice);
        res->rightPos = Node::conditional_select(node->rightPos, res->rightPos, choice);
        for (int k = 0; k < res->key.id.size(); k++) {
            res->key.id[k] = Node::conditional_select(node->key.id[k], res->key.id[k], choice);
        }
        for (int k = 0; k < res->leftID.id.size(); k++) {
            res->leftID.id[k] = Node::conditional_select(node->leftID.id[k], res->leftID.id[k], choice);
        }
        for (int k = 0; k < res->rightID.id.size(); k++) {
            res->rightID.id[k] = Node::conditional_select(node->rightID.id[k], res->rightID.id[k], choice);
        }

        //these 2 should be after result set(here is correct)
        node->leftPos = Node::conditional_select(newChildPos, node->leftPos, !isDummy && match && leftChild);
        node->rightPos = Node::conditional_select(newChildPos, node->rightPos, !isDummy && match && rightChild);

        string tmpRes = "0-0             ";
        for (int i = 0; i < node->value.size(); i++) {
            tmpRes[i] = Node::conditional_select(node->value[i], tmpRes[i], curNodeIsTarget);
        }
        vector<string> parts = split(tmpRes, "-");
        string newval = "";
        if (isFirstPart) {
            newval = to_string(stoi(parts[0]) + 1) + "-" + parts[1];
        } else {
            newval = parts[0] + "-" + to_string(stoi(parts[1]) + 1);
        }
        std::array< byte_t, 16> newVec;
        std::fill(newVec.begin(), newVec.end(), 0);
        std::copy(newval.begin(), newval.end(), newVec.begin());

        for (int k = 0; k < node->value.size(); k++) {
            node->value[k] = Node::conditional_select(newVec[k], node->value[k], curNodeIsTarget && match);
        }

    }

    evict(evictBuckets);
    return res;
}

Node* ORAM::ReadWrite(Bid bid, Node* inputnode, unsigned long long lastLeaf, unsigned long long newLeaf, bool isRead, bool isDummy, std::array< byte_t, 16> value, bool overwrite, bool isIncRead) {
    if (bid == 0) {
        printf("bid is 0 dummy is:%d\n", isDummy ? 1 : 0);
        throw runtime_error("Node id is not set");
    }
    accessCounter++;
    isIncomepleteRead = isIncRead;

    unsigned long long newPos = RandomPath();
    unsigned long long fetchPos = Node::conditional_select(newPos, lastLeaf, isDummy);

    inputnode->pos = fetchPos;

    FetchPath(fetchPos);

    if (!isIncomepleteRead) {
        currentLeaf = fetchPos;
    }

    Node* tmpWrite = Node::clone(inputnode);
    tmpWrite->pos = newLeaf;

    Node* res = new Node();
    res->isDummy = true;
    res->index = nextDummyCounter++;
    res->key = nextDummyCounter++;
    bool write = !isRead;


    vector<Node*> nodesList(stash.nodes.begin(), stash.nodes.end());

    if (isIncomepleteRead) {
        nodesList.insert(nodesList.end(), incStash.nodes.begin(), incStash.nodes.end());
    }

    for (Node* node : nodesList) {
        bool match = Node::CTeq(Bid::CTcmp(node->key, bid), 0) && !node->isDummy;
        node->isDummy = Node::conditional_select(true, node->isDummy, !isDummy && match && write);
        node->pos = Node::conditional_select(newLeaf, node->pos, !isDummy && match);
        for (int k = 0; k < res->value.size(); k++) {
            node->value[k] = Node::conditional_select(value[k], node->value[k], !isDummy && match && overwrite);
        }
        bool choice = !isDummy && match && isRead && !node->isDummy;
        res->index = Node::conditional_select((long long) node->index, (long long) res->index, choice);
        res->isDummy = Node::conditional_select(node->isDummy, res->isDummy, choice);
        res->pos = Node::conditional_select((long long) node->pos, (long long) res->pos, choice);
        for (int k = 0; k < res->value.size(); k++) {
            res->value[k] = Node::conditional_select(node->value[k], res->value[k], choice);
        }
        for (int k = 0; k < res->dum.size(); k++) {
            res->dum[k] = Node::conditional_select(node->dum[k], res->dum[k], choice);
        }
        res->evictionNode = Node::conditional_select(node->evictionNode, res->evictionNode, choice);
        res->height = Node::conditional_select(node->height, res->height, choice);
        res->leftPos = Node::conditional_select(node->leftPos, res->leftPos, choice);
        res->rightPos = Node::conditional_select(node->rightPos, res->rightPos, choice);
        for (int k = 0; k < res->key.id.size(); k++) {
            res->key.id[k] = Node::conditional_select(node->key.id[k], res->key.id[k], choice);
        }
        for (int k = 0; k < res->leftID.id.size(); k++) {
            res->leftID.id[k] = Node::conditional_select(node->leftID.id[k], res->leftID.id[k], choice);
        }
        for (int k = 0; k < res->rightID.id.size(); k++) {
            res->rightID.id[k] = Node::conditional_select(node->rightID.id[k], res->rightID.id[k], choice);
        }
    }

    if (!isIncomepleteRead) {
        stash.insert(tmpWrite);
    } else {
        incStash.insert(tmpWrite);
    }

    if (!isIncomepleteRead) {
        evict(evictBuckets);
    } else {
        for (Node* item : incStash.nodes) {
            delete item;
        }
        incStash.nodes.clear();
    }

    isIncomepleteRead = false;
    return res;
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

void ORAM::finilize(bool noDummyOp) {
    if (!noDummyOp && stashCounter == 100) {
        stashCounter = 0;
        EvictBuckets();
    } else {
        stashCounter++;
    }
}

void ORAM::evict(bool evictBucketsForORAM) {
    double time;
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
        node->evictionNode = Node::conditional_select((long long) 0, node->evictionNode, node->isDummy);
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

    ObliviousOperations::oblixmergesort(&stash.nodes);

    if (profile) {
        ocall_stop_timer(&time, 10);
        printf("First Oblivious Sort: %f\n", time);
        ocall_start_timer(10);
    }

    long long currentID = GetNodeOnPath(currentLeaf, depth);
    int level = depth;
    int counter = 0;

    for (unsigned long long i = 0; i < stash.nodes.size(); i++) {
        Node* curNode = stash.nodes[i];
        bool firstCond = (!Node::CTeq(Node::CTcmp(counter - (depth - level) * Z, Z), -1));
        bool secondCond = Node::CTeq(Node::CTcmp(curNode->evictionNode, currentID), 0);
        bool thirdCond = (!Node::CTeq(Node::CTcmp(counter, Z * depth), -1)) || curNode->isDummy;

        long long tmpEvictionNode = GetNodeOnPath(currentLeaf, depth - (int) floor(counter / Z));
        long long tmpcurrentID = GetNodeOnPath(currentLeaf, level - 1);
        curNode->evictionNode = Node::conditional_select((long long) - 1, curNode->evictionNode, firstCond && secondCond && thirdCond);
        curNode->evictionNode = Node::conditional_select(tmpEvictionNode, curNode->evictionNode, firstCond && secondCond && !thirdCond);
        counter = Node::conditional_select(counter + 1, counter, firstCond && secondCond && !thirdCond);
        counter = Node::conditional_select(counter + 1, counter, !firstCond && secondCond);
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

    if (profile) {
        ocall_stop_timer(&time, 10);
        printf("Sequential Scan on Stash Blocks to assign blocks to blocks:%f\n", time);
        ocall_start_timer(10);
    }

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
            if (virtualStorage.count(curBucketID) != 0) {
                virtualStorage.erase(curBucketID);
            }
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

    if (evictBucketsForORAM) {
        EvictBuckets();
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
}

void ORAM::start(bool isBatchWrite) {
    this->batchWrite = isBatchWrite;
    readCnt = 0;
    accessCounter = 0;
}

void ORAM::beginOperation() {
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

    unsigned int j = 0;
    Bucket* bucket = new Bucket();



    int i;
    printf("Setting Nodes Positions\n");
    for (i = 0; i < nodes->size(); i++) {
        (*nodes)[i]->pos = permutation[i];
        (*nodes)[i]->evictionNode = first_leaf + (*nodes)[i]->pos;
    }
    printf("Adding Dummy Nodes\n");
    unsigned long long neededDummy = ((bucketCount / 2) * Z);
    for (; i < neededDummy; i++) {
        Node* tmp = new Node();
        tmp->index = i + 1;
        tmp->isDummy = true;
        tmp->pos = permutation[i];
        tmp->evictionNode = permutation[i] + first_leaf;
        nodes->push_back(tmp);
    }

    permutation.clear();

    printf("Sorting\n");
    ObliviousOperations::bitonicSort(nodes);

    vector<long long> indexes;
    vector<Bucket> buckets;

    long long first_bucket_of_last_level = bucketCount / 2;

    for (int i = 0; i < first_bucket_of_last_level; i++) {
        if (i % 100000 == 0) {
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


    for (unsigned int i = 0; i < nodes->size(); i++) {
        if (i % 100000 == 0) {
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



    for (int i = 0; i < PERMANENT_STASH_SIZE; i++) {
        Node* tmp = new Node();
        tmp->index = nextDummyCounter;
        tmp->isDummy = true;
        stash.insert(tmp);
    }

}

ORAM::ORAM(long long maxSize, bytes<Key> oram_key, vector<Node*>* nodes)
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

    unsigned int j = 0;
    Bucket* bucket = new Bucket();



    int i;
    printf("Setting Nodes Eviction ID\n");
    for (i = 0; i < nodes->size(); i++) {
        (*nodes)[i]->evictionNode = (*nodes)[i]->pos + first_leaf;
    }

    printf("Sorting\n");
    ObliviousOperations::bitonicSort(nodes);

    vector<long long> indexes;
    vector<Bucket> buckets;

    long long first_bucket_of_last_level = bucketCount / 2;


    for (unsigned int i = 0; i < nodes->size(); i++) {
        if (i % 100000 == 0) {
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

    for (int i = 0; i < first_bucket_of_last_level; i++) {
        if (i % 100000 == 0) {
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

    delete bucket;


    for (int i = 0; i < PERMANENT_STASH_SIZE; i++) {
        Node* tmp = new Node();
        tmp->index = nextDummyCounter;
        tmp->isDummy = true;
        stash.insert(tmp);
    }

}

ORAM::ORAM(long long maxSize, bytes<Key> oram_key, int nodesSize) : key(oram_key) {
    depth = (int) (ceil(log2(maxSize)) - 1) + 1;
    maxOfRandom = (long long) (pow(2, depth));
    //    AES::Setup();
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

    single_block_clen_size = AES::GetCiphertextLength((int) (blockSize));
    single_block_plaintext_size = (blockSize);
    storeSingleBlockSize = single_block_clen_size + IV;
    totalNumberOfNodes = maxOfRandom*Z;
    ocall_setup_ramStore(blockCount, storeBlockSize);
    maxHeightOfAVLTree = (int) floor(log2(blockCount)) + 1;


    ObliviousOperations::totalNumberOfNodes = totalNumberOfNodes;
    ObliviousOperations::blockSize = sizeof (Node);
    ObliviousOperations::single_block_clen_size = single_block_clen_size;
    ObliviousOperations::single_block_plaintext_size = single_block_plaintext_size;
    ObliviousOperations::storeSingleBlockSize = storeSingleBlockSize;
    ObliviousOperations::key = oram_key;
    ObliviousOperations::currentBatchBegin1 = 0;
    ObliviousOperations::currentBatchBegin2 = 0;
    ObliviousOperations::isLeftBatchUpdated = false;

    unsigned long long first_leaf = bucketCount / 2;

    unsigned int k = 0;
    Bucket* bucket = new Bucket();

    printf("Setting Nodes Eviction ID\n");
    for (int i = 0; i < nodesSize; i++) {
        Node* node = getNode(i);
        node->evictionNode = node->pos + first_leaf;
        setNode(i, node);
    }

    flushCache();
    printf("Sorting\n");
    ObliviousOperations::bitonicSort(nodesSize);

    long long first_bucket_of_last_level = bucketCount / 2;

    printf("Writing Buckets\n");

    for (unsigned int j = 0; j <= nodesSize / 10000; j++) {
        char* tmp = new char[10000 * storeBlockSize];
        vector<long long> indexes;
        for (int i = 0; i < min((int) (nodesSize - j * 10000), 10000); i++) {
            Node* cureNode = getNode(i + j * 10000);
            long long curBucketID = cureNode->evictionNode;

            Block &curBlock = (*bucket)[k];
            curBlock.data.resize(blockSize, 0);
            block tmpBlock = convertNodeToBlock(cureNode);
            curBlock.id = Node::conditional_select((unsigned long long) 0, cureNode->index, cureNode->isDummy);
            for (int k = 0; k < tmpBlock.size(); k++) {
                curBlock.data[k] = Node::conditional_select(curBlock.data[k], tmpBlock[k], cureNode->isDummy);
            }
            //delete cureNode;
            k++;

            if (k == Z) {
                indexes.push_back(curBucketID);
                block b = SerialiseBucket((*bucket));
                block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
                std::memcpy(tmp + (indexes.size() - 1) * ciphertext.size(), ciphertext.data(), ciphertext.size());
                delete bucket;
                bucket = new Bucket();
                k = 0;
            }
        }
        if (indexes.size() != 0) {
            ocall_nwrite_ramStore((int) indexes.size(), indexes.data(), (const char*) tmp, storeBlockSize * indexes.size());
        }
        delete tmp;
    }

    printf("Writing Upper Level Buckets\n");

    for (unsigned int j = 0; j <= first_bucket_of_last_level / 10000; j++) {
        char* tmp = new char[10000 * storeBlockSize];
        vector<long long> indexes;
        for (int i = 0; i < min((int) (first_bucket_of_last_level - j * 10000), 10000); i++) {
            for (int z = 0; z < Z; z++) {
                Block &curBlock = (*bucket)[z];
                curBlock.id = 0;
                curBlock.data.resize(blockSize, 0);
            }
            indexes.push_back(i + j * 10000);
            block b = SerialiseBucket((*bucket));
            block ciphertext = AES::Encrypt(key, b, clen_size, plaintext_size);
            std::memcpy(tmp + i * ciphertext.size(), ciphertext.data(), ciphertext.size());
            delete bucket;
            bucket = new Bucket();
        }
        if (indexes.size() != 0) {
            ocall_nwrite_ramStore(indexes.size(), indexes.data(), (const char*) tmp, storeBlockSize * indexes.size());
        }
        delete tmp;
    }

    delete bucket;

    for (int i = 0; i < PERMANENT_STASH_SIZE; i++) {
        Node* tmp = new Node();
        tmp->index = nextDummyCounter;
        tmp->isDummy = true;
        stash.insert(tmp);
    }
}

void ORAM::setNode(int index, Node* node) {
    //        size_t readSize;
    //        unsigned long long plaintext_size = sizeof (Node);
    //    
    //        char* tmp = new char[storeSingleBlockSize];
    //    
    //        std::array<byte_t, sizeof (Node) > data;
    //    
    //        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*node)));
    //        const byte_t* end = begin + sizeof (Node);
    //        std::copy(begin, end, std::begin(data));
    //    
    //        block buffer(data.begin(), data.end());
    //    block ciphertext = AES::Encrypt(key, buffer, single_block_clen_size, single_block_plaintext_size);
    //        std::memcpy(tmp, ciphertext.data(), storeSingleBlockSize);
    //        delete node;
    //    
    //        ocall_write_rawRamStore(index, (const char*) tmp, storeSingleBlockSize);
    //        delete tmp;
}

Node* ORAM::getNode(int index) {
    //            char* tmp = new char[storeSingleBlockSize];
    //            size_t readSize;
    //            ocall_read_rawRamStore(&readSize, index, tmp, storeSingleBlockSize);
    //            block ciphertext(tmp, tmp + storeSingleBlockSize);
    //    block buffer = AES::Decrypt(key, ciphertext, single_block_clen_size);
    //        
    //            Node* node = new Node();
    //            std::array<byte_t, sizeof (Node) > arr;
    //            std::copy(buffer.begin(), buffer.begin() + sizeof (Node), arr.begin());
    //            from_bytes(arr, *node);
    //            delete tmp;
    //            return node;


    if ((index >= currentBatchBegin1 && index < currentBatchBegin1 + BATCH_SIZE) && (setupCache1.size() != 0)) {
        return setupCache1[index - currentBatchBegin1];
    } else if ((index >= currentBatchBegin2 && index < currentBatchBegin2 + BATCH_SIZE) && (setupCache2.size() != 0)) {
        return setupCache2[index - currentBatchBegin2];
    } else {
        int pageBegin = (index / BATCH_SIZE) * BATCH_SIZE;
        if (isLeftBatchUpdated == false) {
            isLeftBatchUpdated = true;
            fetchBatch1(pageBegin);
            return setupCache1[index - currentBatchBegin1];
        } else {
            isLeftBatchUpdated = false;
            fetchBatch2(pageBegin);
            return setupCache2[index - currentBatchBegin2];
        }
    }
}

void ORAM::flushCache() {
    size_t readSize;

    char* tmp = new char[setupCache1.size() * storeSingleBlockSize];
    vector<long long> indexes;
    for (int j = 0; j < setupCache1.size(); j++) {
        indexes.push_back(currentBatchBegin1 + j);

        std::array<byte_t, sizeof (Node) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(setupCache1[j]))));
        const byte_t* end = begin + sizeof (Node);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(key, buffer, single_block_clen_size, single_block_plaintext_size);
        std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), storeSingleBlockSize);
        delete setupCache1[j];
    }

    if (setupCache1.size() != 0) {
        ocall_nwrite_rawRamStore(setupCache1.size(), indexes.data(), (const char*) tmp, storeSingleBlockSize * setupCache1.size());
    }

    setupCache1.clear();
    delete tmp;
    indexes.clear();

    tmp = new char[setupCache2.size() * storeSingleBlockSize];
    for (int j = 0; j < setupCache2.size(); j++) {
        indexes.push_back(currentBatchBegin2 + j);

        std::array<byte_t, sizeof (Node) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(setupCache2[j]))));
        const byte_t* end = begin + sizeof (Node);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(key, buffer, single_block_clen_size, single_block_plaintext_size);
        std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), storeSingleBlockSize);
        delete setupCache2[j];
    }

    if (setupCache2.size() != 0) {
        ocall_nwrite_rawRamStore(setupCache2.size(), indexes.data(), (const char*) tmp, storeSingleBlockSize * setupCache2.size());
    }

    setupCache2.clear();
    delete tmp;
}

void ORAM::fetchBatch1(int beginIndex) {
    size_t readSize;

    char* tmp = new char[setupCache1.size() * storeSingleBlockSize];
    vector<long long> indexes;
    for (int j = 0; j < setupCache1.size(); j++) {
        indexes.push_back(currentBatchBegin1 + j);

        std::array<byte_t, sizeof (Node) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(setupCache1[j]))));
        const byte_t* end = begin + sizeof (Node);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(key, buffer, single_block_clen_size, single_block_plaintext_size);
        std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), storeSingleBlockSize);
        delete setupCache1[j];
    }

    if (setupCache1.size() != 0) {
        ocall_nwrite_rawRamStore(setupCache1.size(), indexes.data(), (const char*) tmp, storeSingleBlockSize * setupCache1.size());
    }
    delete tmp;

    setupCache1.clear();


    tmp = new char[BATCH_SIZE * storeSingleBlockSize];
    ocall_nread_rawRamStore(&readSize, BATCH_SIZE, beginIndex, tmp, BATCH_SIZE * storeSingleBlockSize);
    for (unsigned int i = 0; i < min((unsigned long long) BATCH_SIZE, totalNumberOfNodes - beginIndex); i++) {
        block ciphertext(tmp + i*storeSingleBlockSize, tmp + (i + 1) * storeSingleBlockSize);
        block buffer = AES::Decrypt(key, ciphertext, single_block_clen_size);

        Node* node = new Node();
        std::array<byte_t, sizeof (Node) > arr;
        std::copy(buffer.begin(), buffer.begin() + sizeof (Node), arr.begin());
        from_bytes(arr, *node);
        setupCache1.push_back(node);
    }
    currentBatchBegin1 = beginIndex;
    delete tmp;
}

void ORAM::fetchBatch2(int beginIndex) {
    size_t readSize;

    char* tmp = new char[setupCache2.size() * storeSingleBlockSize];
    vector<long long> indexes;
    for (int j = 0; j < setupCache2.size(); j++) {
        indexes.push_back(currentBatchBegin2 + j);

        std::array<byte_t, sizeof (Node) > data;

        const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof((*(setupCache2[j]))));
        const byte_t* end = begin + sizeof (Node);
        std::copy(begin, end, std::begin(data));

        block buffer(data.begin(), data.end());
        block ciphertext = AES::Encrypt(key, buffer, single_block_clen_size, single_block_plaintext_size);
        std::memcpy(tmp + j * ciphertext.size(), ciphertext.data(), storeSingleBlockSize);
        delete setupCache2[j];
    }

    if (setupCache2.size() != 0) {
        ocall_nwrite_rawRamStore(setupCache2.size(), indexes.data(), (const char*) tmp, storeSingleBlockSize * setupCache2.size());
    }
    delete tmp;

    setupCache2.clear();


    tmp = new char[BATCH_SIZE * storeSingleBlockSize];
    ocall_nread_rawRamStore(&readSize, BATCH_SIZE, beginIndex, tmp, BATCH_SIZE * storeSingleBlockSize);
    for (unsigned int i = 0; i < min((unsigned long long) BATCH_SIZE, totalNumberOfNodes - beginIndex); i++) {
        block ciphertext(tmp + i*storeSingleBlockSize, tmp + (i + 1) * storeSingleBlockSize);
        block buffer = AES::Decrypt(key, ciphertext, single_block_clen_size);

        Node* node = new Node();
        std::array<byte_t, sizeof (Node) > arr;
        std::copy(buffer.begin(), buffer.begin() + sizeof (Node), arr.begin());
        from_bytes(arr, *node);
        setupCache2.push_back(node);
    }
    currentBatchBegin2 = beginIndex;
    delete tmp;
}

void ORAM::setupInsert(vector<Node*>* nodes) {

    unsigned long long first_leaf = bucketCount / 2;

    unsigned int j = 0;
    Bucket* bucket = new Bucket();



    int i;
    printf("Setting Nodes Eviction ID\n");
    for (i = 0; i < nodes->size(); i++) {
        (*nodes)[i]->evictionNode = (*nodes)[i]->pos + first_leaf;
    }

    printf("Sorting\n");
    ObliviousOperations::bitonicSort(nodes);


    vector<long long> indexes;
    vector<Bucket> buckets;

    long long first_bucket_of_last_level = bucketCount / 2;

    for (int i = 0; i < first_bucket_of_last_level; i++) {
        if (i % 100000 == 0) {
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


    for (unsigned int i = 0; i < nodes->size(); i++) {
        if (i % 100000 == 0) {
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



    for (int i = 0; i < PERMANENT_STASH_SIZE; i++) {
        Node* tmp = new Node();
        tmp->index = nextDummyCounter;
        tmp->isDummy = true;
        tmp->pos = RandomPath();
        stash.insert(tmp);
    }
}