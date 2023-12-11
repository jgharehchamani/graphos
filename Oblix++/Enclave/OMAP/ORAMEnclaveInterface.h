#ifndef ORAMENCLAVEINTERFACE_H
#define ORAMENCLAVEINTERFACE_H

#include "../Enclave.h"
#include "Enclave_t.h"
#include <string>
#include "ORAM.hpp"

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

double ecall_measure_oram_speed(int testSize) {
    bytes<Key> tmpkey{0};
    double time1, time2, total = 0;
    int depth = (int) (ceil(log2(testSize)) - 1) + 1;
    int maxSize = (int) (pow(2, depth));

    map<unsigned long long, unsigned long long> PositionsMap;
    ORAM* oram = new ORAM(maxSize, tmpkey, false);

    for (int i = 0; i < maxSize; i++) {
        PositionsMap[i] = i;
    }

    check_memory("begin");
    printf("Warming up the ORAM by dummy operations:\n");
    for (int i = 1; i <= 10000; i++) {
        if (i % 1000 == 0) {
            printf("%d/10000\n",i);
        }
        uint32_t randval;
        sgx_read_rand((unsigned char *) &randval, 4);
        unsigned long long id = (i % (maxSize - 1)) + 1;
        unsigned long long value = i;
        long long pos;
        if (PositionsMap.count(id) == 0) {
            pos = id;
        } else {
            pos = PositionsMap[id];
        }
        oram->start(false);
        pos = oram->Access(id, pos, value);
        oram->start(false);
        unsigned long long res = 0;
        pos = oram->Access(id, pos, res);
        PositionsMap[id] = pos;
        assert(value == res);
    }

    for (int i = 0; i < oram->times.size(); i++) {
        oram->times[i].clear();
    }
    oram->beginProfile = true;

    for (int j = 0; j < 10; j++) {
        total = 0;
        for (int i = 1; i <= 1; i++) {
            uint32_t randval;
            sgx_read_rand((unsigned char *) &randval, 4);
            unsigned long long id = (i % (maxSize - 1)) + 1;
            unsigned long long value = i;
            long long pos;
            if (PositionsMap.count(id) == 0) {
                pos = id;
            } else {
                pos = PositionsMap[id];
            }
            ocall_start_timer(535);
            oram->start(false);
            pos = oram->Access(id, pos, value);
            ocall_stop_timer(&time1, 535);
            unsigned long long res = 0;
            ocall_start_timer(535);
            oram->start(false);
            pos = oram->Access(id, pos, res);
            ocall_stop_timer(&time2, 535);
            PositionsMap[id] = pos;
            assert(value == res);
            total += time1 + time2;
        }
        printf("Total Access Time: %f\n", total / 2);
    }
    vector<string> names;
    names.push_back("Fetch:");
    names.push_back("Sort:");
    names.push_back("Sequential Scan:");
    names.push_back("Write Buckets:");
    names.push_back("Eviction:");

    for (int i = 0; i < names.size(); i++) {
        printf("%s\n", names[i].c_str());
        for (int j = 0; j < oram->times[i].size(); j++) {
            printf("%f\n", oram->times[i][j]);
        }
    }
    return oram->evicttime / oram->evictcount;
}

double ecall_measure_setup_speed(int testSize) {
    vector<Node*> nodes;
    int depth = (int) (ceil(log2(testSize)) - 1) + 1;
    long long maxSize = (int) (pow(2, depth));
    for (int i = 0; i < maxSize; i++) {
        Node* tmp = new Node();
        tmp->index = i + 1;
        tmp->value = i + 1;
        tmp->isDummy = false;
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
    printf("Setup time is:%f\n", time2);

    //    vector<string> names;
    //    names.push_back("Add Dummy:");
    //    names.push_back("Sort:");
    //    names.push_back("Add Upper Level Dummy:");
    //    names.push_back("Write Buckets:");
    //
    //    for (int i = 0; i < names.size(); i++) {
    //        printf("%s\n", names[i].c_str());
    //        for (int j = 0; j < oram->times[i].size(); j++) {
    //            printf("%f\n", oram->times[i][j]);
    //        }
    //    }
}

double ecall_measure_eviction_speed(int testSize) {
    bytes<Key> tmpkey{0};
    double time1, total = 0;
    long long s = (long long) pow(2, testSize);
    ORAM* oram = new ORAM(s, tmpkey, true);

    for (int j = 0; j < 10; j++) {
        total = 0;
        for (int i = 1; i <= 500; i++) {
            oram->start(false);
            oram->prepareForEvictionTest();
            ocall_start_timer(535);
            oram->evict(true);
            ocall_stop_timer(&time1, 535);
            total += time1;
        }
        printf("Total Eviction Time: %f\n", total / 500);
    }
    return oram->evicttime / oram->evictcount;
}
#endif /* ORAMENCLAVEINTERFACE_H */

