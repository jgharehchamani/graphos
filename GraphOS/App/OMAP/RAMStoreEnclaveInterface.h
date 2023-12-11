#ifndef RAMSTOREENCLAVEINTERFACE_H
#define RAMSTOREENCLAVEINTERFACE_H
#include "RAMStore.hpp"
#include "Utilities.h"

RAMStore* runStore = NULL;
RAMStore* setupStore = NULL;
RAMStore* heapStore = NULL;

bool setupMode = false;

void ocall_setup_heapStore(size_t num, int size) {
    if (heapStore == NULL) {
            heapStore = new RAMStore(num, false);
    }
}

void ocall_setup_ramStore(size_t num, int size) {
    if (setupMode) {
        if (setupStore == NULL) {
            if (size != -1) {
                setupStore = new RAMStore(num, false);
            } else {
                setupStore = new RAMStore(num, true);
            }
        }
    } else {
        if (runStore == NULL) {
            if (size != -1) {
                runStore = new RAMStore(num, false);
            } else {
                runStore = new RAMStore(num, true);
            }
        }
    }
}

void ocall_nwrite_ramStore(size_t blockCount, long long* indexes, const char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t eachSize = len / blockCount;
    for (unsigned int i = 0; i < blockCount; i++) {
        block ciphertext(blk + (i * eachSize), blk + (i + 1) * eachSize);
        if (setupMode) {
            setupStore->Write(indexes[i], ciphertext);
        } else {
            runStore->Write(indexes[i], ciphertext);
        }
    }
}

void ocall_nwrite_heapStore(size_t blockCount, long long* indexes, const char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t eachSize = len / blockCount;
    for (unsigned int i = 0; i < blockCount; i++) {
        block ciphertext(blk + (i * eachSize), blk + (i + 1) * eachSize);
        heapStore->Write(indexes[i], ciphertext);
    }
}

void ocall_write_rawRamStore(long long index, const char *blk, size_t len) {
    size_t eachSize = len;
    block ciphertext(blk, blk + eachSize);
    if (setupMode) {
        setupStore->WriteRawStore(index, ciphertext);
    } else {
        runStore->WriteRawStore(index, ciphertext);
    }
}

void ocall_write_prfRamStore(long long index, const char *blk, size_t len) {
    size_t eachSize = len;
    block ciphertext(blk, blk + eachSize);
    if (setupMode) {
        setupStore->WritePRF(index, ciphertext);
    } else {
        runStore->WritePRF(index, ciphertext);
    }
}

void ocall_nwrite_rawRamStore(size_t blockCount, long long* indexes, const char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t eachSize = len / blockCount;
    for (unsigned int i = 0; i < blockCount; i++) {
        block ciphertext(blk + (i * eachSize), blk + (i + 1) * eachSize);
        if (setupMode) {
            setupStore->WriteRawStore(indexes[i], ciphertext);
        } else {
            runStore->WriteRawStore(indexes[i], ciphertext);
        }
    }
}

void ocall_nwrite_ramStore_by_client(vector<long long>* indexes, vector<block>* ciphertexts) {
    for (unsigned int i = 0; i < (*indexes).size(); i++) {
        if (setupMode) {
            setupStore->Write((*indexes)[i], (*ciphertexts)[i]);
        } else {
            runStore->Write((*indexes)[i], (*ciphertexts)[i]);
        }
    }
}

void ocall_nwrite_raw_ramStore(vector<block>* ciphertexts) {
    for (unsigned int i = 0; i < (*ciphertexts).size(); i++) {
        if (setupMode) {
            setupStore->WriteRawStore(i, (*ciphertexts)[i]);
        } else {
            runStore->WriteRawStore(i, (*ciphertexts)[i]);
        }
    }
}

void ocall_finish_setup() {
    setupMode = false;
}

void ocall_begin_setup() {
    setupMode = true;
}

void ocall_nwrite_rawRamStore_for_graph(size_t blockCount, long long* indexes, const char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t eachSize = len / blockCount;
    for (unsigned int i = 0; i < blockCount; i++) {
        block ciphertext(blk + (i * eachSize), blk + (i + 1) * eachSize);
        runStore->WriteRawStore(indexes[i], ciphertext);
    }
}

size_t ocall_nread_ramStore(size_t blockCount, long long* indexes, char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t resLen = -1;
    for (unsigned int i = 0; i < blockCount; i++) {
        block ciphertext = setupMode ? setupStore->Read(indexes[i]) : runStore->Read(indexes[i]);
        resLen = ciphertext.size();
        std::memcpy(blk + i * resLen, ciphertext.data(), ciphertext.size());
    }
    return resLen;
}

size_t ocall_nread_heapStore(size_t blockCount, long long* indexes, char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t resLen = -1;
    for (unsigned int i = 0; i < blockCount; i++) {
        block ciphertext = heapStore->Read(indexes[i]);
        resLen = ciphertext.size();
        std::memcpy(blk + i * resLen, ciphertext.data(), ciphertext.size());
    }
    return resLen;
}

size_t ocall_read_rawRamStore(size_t index, char *blk, size_t len) {
    size_t resLen = -1;
    block ciphertext = setupMode ? setupStore->ReadRawStore(index) : runStore->ReadRawStore(index);
    resLen = ciphertext.size();
    std::memcpy(blk, ciphertext.data(), ciphertext.size());
    return resLen;
}

size_t ocall_read_prfRamStore(size_t index, char *blk, size_t len) {
    size_t resLen = -1;
    block ciphertext = setupMode ? setupStore->ReadPRF(index) : runStore->ReadPRF(index);
    resLen = ciphertext.size();
    std::memcpy(blk, ciphertext.data(), ciphertext.size());
    return resLen;
}

size_t ocall_nread_rawRamStore(size_t blockCount, size_t begin, char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t resLen = -1;
    size_t rawSize = setupMode ? setupStore->tmpstore.size() : runStore->tmpstore.size();
    size_t i;
    for (i = 0; i < blockCount && (begin + i) < rawSize; i++) {
        block ciphertext = setupMode ? setupStore->ReadRawStore(i + begin) : runStore->ReadRawStore(i + begin);
        size_t Len = ciphertext.size();
        std::memcpy(blk + i * Len, ciphertext.data(), ciphertext.size());
    }
    return i;
}

size_t ocall_nread_prf(size_t blockCount, size_t begin, char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t resLen = -1;
    size_t rawSize = setupMode ? setupStore->prfstore.size() : runStore->prfstore.size();
    for (unsigned int i = 0; i < blockCount && (begin + i) < rawSize; i++) {
        block ciphertext = setupMode ? setupStore->ReadPRF(i + begin) : runStore->ReadPRF(i + begin);
        resLen = ciphertext.size();
        std::memcpy(blk + i * resLen, ciphertext.data(), ciphertext.size());
    }
    return resLen;
}

void ocall_initialize_ramStore(long long begin, long long end, const char *blk, size_t len) {
    block ciphertext(blk, blk + len);
    for (long long i = begin; i < end; i++) {
        if (setupMode) {
            setupStore->Write(i, ciphertext);
        } else {
            runStore->Write(i, ciphertext);
        }
    }
}

void ocall_initialize_heapStore(long long begin, long long end, const char *blk, size_t len) {
    block ciphertext(blk, blk + len);
    for (long long i = begin; i < end; i++) {
        heapStore->Write(i, ciphertext);
    }
}

void ocall_write_ramStore(long long index, const char *blk, size_t len) {
    block ciphertext(blk, blk + len);
    if (setupMode) {
        setupStore->Write(index, ciphertext);
    } else {
        runStore->Write(index, ciphertext);
    }
}

void ocall_write_heapStore(long long index, const char *blk, size_t len) {
    block ciphertext(blk, blk + len);
    heapStore->Write(index, ciphertext);
}

void ocall_nwrite_prf(size_t blockCount, long long* indexes, const char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t eachSize = len / blockCount;
    for (unsigned int i = 0; i < blockCount; i++) {
        block ciphertext(blk + (i * eachSize), blk + (i + 1) * eachSize);
        if (setupMode) {
            setupStore->WritePRF(indexes[i], ciphertext);
        } else {
            runStore->WritePRF(indexes[i], ciphertext);
        }
    }
}

#endif /* RAMSTOREENCLAVEINTERFACE_H */

