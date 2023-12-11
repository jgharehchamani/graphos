#ifndef ORAMENCLAVEINTERFACE_H
#define ORAMENCLAVEINTERFACE_H

#include "../Enclave.h"
#include "Enclave_t.h"
#include "OMAP.h"
//#include "OHeap.h"
#include "DOHEAP.hpp"
#include <string>

static OMAP* omap = NULL;
static DOHEAP* oheap = NULL;
//static OHeap* oheap = NULL;

map<string, string> setupPairs;
bool setup = false;

void ecall_setup_oram(int max_size) {
    bytes<Key> tmpkey{0};
    omap = new OMAP(max_size, tmpkey, false);
}

void ecall_setup_omap_by_client(int max_size, const char *bid, long long rootPos, const char* secretKey) {
    bytes<Key> tmpkey;
    std::memcpy(tmpkey.data(), secretKey, Key);
    std::array<byte_t, ID_SIZE> id;
    std::memcpy(id.data(), bid, ID_SIZE);
    Bid rootBid(id);
    omap = new OMAP(max_size, rootBid, rootPos, tmpkey);
}

void ecall_setup_omap_with_small_memory(int max_size, long long initialSize, const char* secretKey) {
    bytes<Key> tmpkey;
    std::memcpy(tmpkey.data(), secretKey, Key);
    omap = new OMAP(max_size, initialSize, tmpkey);
}

void ecall_setup_oheap(int maxSize) {
    bytes<Key> tmpkey{0};
    oheap = new DOHEAP(maxSize, tmpkey, false);
    //        oheap = new OHeap(omap, maxSize);
}

void ecall_set_new_minheap_node(int newMinHeapNodeV, int newMinHeapNodeDist) {
    //    oheap->setNewMinHeapNode(newMinHeapNodeV, newMinHeapNodeDist);
    Bid id = newMinHeapNodeDist;
    array<byte_t, 16> value;
    std::fill(value.begin(), value.end(), 0);
    for (int i = 0; i < 4; i++) {
        value[i] = (byte_t) (newMinHeapNodeV >> (i * 8));
    }
    oheap->execute(id, value, 2);
}

void ecall_execute_heap_operation(int* v, int* dist, int op) {
    //        oheap->execute(*v, *dist, op);
    int d = *dist;
    Bid id = d;
    int val = *v;
    array<byte_t, 16> value;
    std::fill(value.begin(), value.end(), 0);
    for (int i = 0; i < 4; i++) {
        value[i] = (byte_t) (val >> (i * 8));
    }
    pair<Bid, array<byte_t, 16> > res = oheap->execute(id, value, op);
    int rr = res.first.getValue();
    *dist = rr;
    std::memcpy(v, res.second.data(), sizeof (int));
}

void ecall_dummy_heap_op() {
    //    oheap->dummyOperation();
}

void ecall_extract_min_id(int* id, int* dist) {
    //    oheap->extractMinID(*id, *dist);
}

void ecall_read_node(const char *bid, char* value) {
    string res;
    if (setup) {
        string curkey(bid);
        res = setupPairs[curkey];
    } else {
        std::array<byte_t, ID_SIZE> id;
        std::memcpy(id.data(), bid, ID_SIZE);
        Bid inputBid(id);
        res = omap->find(inputBid);
    }
    std::memcpy(value, res.c_str(), 16);
}

void ecall_read_and_set_node(const char *bid, char* value) {
    std::array<byte_t, ID_SIZE> id;
    std::memcpy(id.data(), bid, ID_SIZE);
    Bid inputBid(id);
    string res = omap->setSpt(inputBid);
    std::memcpy(value, res.c_str(), 16);
}

void ecall_read_write_node(const char *bid, const char* value, char* oldValue) {
    std::array<byte_t, ID_SIZE> id;
    std::memcpy(id.data(), bid, ID_SIZE);
    Bid inputBid(id);
    string val(value);
    omap->searchInsert(inputBid, val);
}

void ecall_write_node(const char *bid, const char* value) {
    if (setup) {
        string curKey(bid);
        string val(value);
        setupPairs[curKey] = val;
    } else {
        std::array<byte_t, ID_SIZE> id;
        std::memcpy(id.data(), bid, ID_SIZE);
        Bid inputBid(id);
        string val(value);
        omap->insert(inputBid, val);
    }
}

void ecall_start_setup() {
    setup = true;
}

void ecall_end_setup() {
    setup = false;
    map<Bid, string>* pairs = new map<Bid, string>();
    for (auto item : setupPairs) {
        std::array<byte_t, ID_SIZE> id;
        std::memcpy(id.data(), item.first.data(), ID_SIZE);
        Bid inputBid(id);
        (*pairs)[inputBid] = item.second;
    }
    omap->setupInsert((*pairs));
    setupPairs.clear();
}

void ecall_read_and_set_dist_node(const char *bid, const char* value, char* result) {
    std::array<byte_t, ID_SIZE> id;
    std::memcpy(id.data(), bid, ID_SIZE);
    Bid inputBid(id);
    string val(value);
    string res = omap->readAndSetDist(inputBid, val);
    std::memcpy(result, res.c_str(), 16);
}

double ecall_measure_oram_speed(int testSize) {
    bytes<Key> tmpkey{0};
    double time1, time2, total = 0;
    int depth = (int) (ceil(log2(testSize)) - 1) + 1;
    int maxSize = (int) (pow(2, depth));
    Node* dummyNode = new Node();
    dummyNode->isDummy = true;
    ORAM* oram = new ORAM(testSize, tmpkey, false);
    map<unsigned long long, unsigned long long> PositionsMap;
    for (int i = 1; i <= 10000; i++) {
        if (i % 1000 == 0) {
            printf("%d/%d\n", i, 10000);
        }
        unsigned long long index = (i % (maxSize - 1)) + 1;
        uint32_t randval;
        sgx_read_rand((unsigned char *) &randval, 4);
        Node* node = new Node();
        Bid id;
        id.setValue(index);
        node->key = id;
        unsigned long long pos;
        if (PositionsMap.count(index) == 0) {
            pos = index;
        } else {
            pos = PositionsMap[index];
        }
        node->pos = pos;
        string value = "test_" + to_string(i);
        std::fill(node->value.begin(), node->value.end(), 0);
        std::copy(value.begin(), value.end(), node->value.begin());
        node->leftID = 0;
        node->index = index;
        node->leftPos = -1;
        node->rightPos = -1;
        node->rightID = 0;
        node->isDummy = false;
        node->height = 1;
        unsigned long long readpos = node->pos;
        unsigned long long dumypos;
        oram->start(false);
        oram->ReadWrite(id, node, pos, pos, false, false, false);
        oram->start(false);
        Node* tmpNode = NULL;
        Node* res = oram->ReadWrite(id, dummyNode, readpos, readpos, true, false, false);
        string resStr = "";
        resStr.assign(res->value.begin(), res->value.end());
        resStr = resStr.c_str();
        delete res;
        PositionsMap[index] = pos;
        assert(value == resStr);
    }
    oram->evicttime = 0;
    oram->evictcount = 0;

    for (int j = 0; j < 5; j++) {
        total = 0;
        for (int i = 1; i <= 500; i++) {
            if (i % 1000 == 0) {
                printf("%d/%d\n", i, 500);
            }
            unsigned long long index = (i % (maxSize - 1)) + 1;
            uint32_t randval;
            sgx_read_rand((unsigned char *) &randval, 4);
            Node* node = new Node();
            Bid id;
            id.setValue(index);
            node->key = id;
            unsigned long long pos;
            if (PositionsMap.count(index) == 0) {
                pos = index;
            } else {
                pos = PositionsMap[index];
            }
            node->pos = pos;
            string value = "test_" + to_string(i);
            std::fill(node->value.begin(), node->value.end(), 0);
            std::copy(value.begin(), value.end(), node->value.begin());
            node->leftID = 0;
            node->index = index;
            node->leftPos = -1;
            node->rightPos = -1;
            node->rightID = 0;
            node->isDummy = false;
            node->height = 1;
            unsigned long long readpos = node->pos;
            unsigned long long dumypos;
            ocall_start_timer(535);
            oram->start(false);
            oram->ReadWrite(id, node, pos, pos, false, false, false);
            ocall_stop_timer(&time1, 535);
            ocall_start_timer(535);
            oram->start(false);
            Node* tmpNode = NULL;
            Node* res = oram->ReadWrite(id, dummyNode, readpos, readpos, true, false, false);
            ocall_stop_timer(&time2, 535);
            string resStr = "";
            resStr.assign(res->value.begin(), res->value.end());
            resStr = resStr.c_str();
            delete res;
            PositionsMap[index] = pos;
            assert(value == resStr);
            total += time1 + time2;
        }
        printf("Total Access Time: %f\n", total / 1000);
    }
    return oram->evicttime / oram->evictcount;
}

void check_memory(string text) {
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

double ecall_measure_omap_speed(int testSize) {
    double time1, time2, total = 0;
    ecall_setup_oram(testSize);
    printf("Warming UP DOMAP:\n");
    for (int i = 0; i < 2000; i++) {
        if (i % 10 == 0) {
            check_memory(to_string(i));
        }
        total = 0;
        uint32_t randval;
        sgx_read_rand((unsigned char *) &randval, 4);
        int num = (randval % (testSize)) + 1;
        std::array< uint8_t, 16> id;
        std::fill(id.begin(), id.end(), 0);

        for (int j = 0; j < 4; j++) {
            id[3 - j] = (byte_t) (num >> (j * 8));
        }

        string str = to_string(num);
        std::array< uint8_t, 16> value;
        std::fill(value.begin(), value.end(), 0);
        std::copy(str.begin(), str.end(), value.begin());
        ocall_start_timer(535);
        ecall_write_node((const char*) id.data(), (const char*) value.data());
        ocall_stop_timer(&time1, 535);

        char* val = new char[16];
        ocall_start_timer(535);
        ecall_read_node((const char*) id.data(), val);
        ocall_stop_timer(&time2, 535);
        total += time1 + time2;
        assert(string(val) == str);
        delete val;
    }


    for (int j = 0; j < 10; j++) {
        total = 0;
        for (int i = 1; i <= 10; i++) {
            uint32_t randval;
            sgx_read_rand((unsigned char *) &randval, 4);
            int num = (randval % (testSize)) + 1;
            std::array< uint8_t, 16> id;
            std::fill(id.begin(), id.end(), 0);

            for (int j = 0; j < 4; j++) {
                id[3 - j] = (byte_t) (num >> (j * 8));
            }

            string str = to_string(i);
            std::array< uint8_t, 16> value;
            std::fill(value.begin(), value.end(), 0);
            std::copy(str.begin(), str.end(), value.begin());
            ocall_start_timer(535);
            ecall_write_node((const char*) id.data(), (const char*) value.data());
            ocall_stop_timer(&time1, 535);
            printf("Write Time:%f\n", time1);
            char* val = new char[16];
            ocall_start_timer(535);
            ecall_read_node((const char*) id.data(), val);
            ocall_stop_timer(&time2, 535);
            printf("Read Time:%f\n", time2);
            total += time1 + time2;
            //printf("expected value:%s result:%s\n",str.c_str(),string(val).c_str());
            assert(string(val) == str);
            delete val;
        }
        printf("Average OMAP Access Time: %f\n", total / 200);
    }


    return total / (20);
}

double ecall_measure_eviction_speed(int testSize) {
    bytes<Key> tmpkey{0};
    double time1, total = 0;
    long long s = (long long) pow(2, testSize);
    ORAM* oram = new ORAM(s, tmpkey, true, true);

    for (int j = 0; j < 10; j++) {
        total = 0;
        for (int i = 1; i <= 100; i++) {
            oram->start(false);
            oram->prepareForEvictionTest();
            ocall_start_timer(535);
            oram->evict(true);
            ocall_stop_timer(&time1, 535);
            total += time1;
        }
        printf("Total Eviction Time: %f\n", total / 100);
    }
    return oram->evicttime / oram->evictcount;
}

double ecall_measure_oram_setup_speed(int testSize) {
    vector<Node*> nodes;
    int depth = (int) (ceil(log2(testSize)) - 1) + 1;
    long long maxSize = (int) (pow(2, depth));
    for (int i = 0; i < maxSize; i++) {
        Node* tmp = new Node();
        tmp->index = i + 1;
        tmp->key = i + 1;
        tmp->isDummy = false;
        string value = "test_" + to_string(i + 1);
        std::fill(tmp->value.begin(), tmp->value.end(), 0);
        std::copy(value.begin(), value.end(), tmp->value.begin());
        uint32_t randval;
        sgx_read_rand((unsigned char *) &randval, 4);
        nodes.push_back(tmp);
    }
    map<unsigned long long, unsigned long long> permutation;

    int j = 0;
    int cnt = 0;
    for (int i = 0; i < maxSize * 4; i++) {
        if (i % 1000000 == 0) {
            printf("%d/%d\n", i, maxSize * 4);
        }
        if (cnt == 4) {
            j++;
            cnt = 0;
        }
        permutation[i] = (j + 1) % maxSize;
        cnt++;
    }

    bytes<Key> tmpkey{0};
    double time2;
    ocall_start_timer(535);
    ORAM* oram = new ORAM(maxSize, tmpkey, &nodes, permutation);
    ocall_stop_timer(&time2, 535);
    //    Node* dummyNode = new Node();
    //    dummyNode->isDummy = true;
    //    Bid id = 2;
    //    unsigned long long readpos = 1;
    //    Node* res = oram->ReadWrite(id, dummyNode, readpos, readpos, true, false);
    //     assert(!res->isDummy);
    printf("Setup time is:%f\n", time2);
}

double ecall_measure_omap_setup_speed(int testSize) {
    map<Bid, string> pairs;
    int depth = (int) (ceil(log2(testSize)) - 1) + 1;
    long long maxSize = (int) (pow(2, depth));
    for (int i = 1; i < maxSize / 10; i++) {
        Bid k = i;
        pairs[k] = "test_" + to_string(i);
    }
    map<unsigned long long, unsigned long long> permutation;

    int j = 0;
    int cnt = 0;
    for (int i = 0; i < maxSize * 4; i++) {
        if (i % 1000000 == 0) {
            printf("%d/%d\n", i, maxSize * 4);
        }
        if (cnt == 4) {
            j++;
            cnt = 0;
        }
        permutation[i] = (j + 1) % maxSize;
        cnt++;
    }

    bytes<Key> tmpkey{0};
    double time2;
    ocall_start_timer(535);
    OMAP* omap = new OMAP(maxSize, tmpkey, &pairs, &permutation);
    ocall_stop_timer(&time2, 535);
    Bid testKey = 3;
    string res = omap->find(testKey);
    assert(strcmp(res.c_str(), "test_3") == 0);
    printf("Setup time is:%f\n", time2);

    //    printf("Creating AVL time is:%f\n", omap->treeHandler->times[0][0]);
    //    printf("ORAM Setup:%f\n", omap->treeHandler->times[1][0]);
}
#endif /* ORAMENCLAVEINTERFACE_H */

