#ifndef RAMSTOREENCLAVEINTERFACE_H
#define RAMSTOREENCLAVEINTERFACE_H
#include "RAMStore.hpp"
#include "Utilities.h"

static RAMStore* store = NULL;

void ocall_setup_ramStore(size_t num, int size) {
    if (store == NULL) {
        if (size != -1) {
            store = new RAMStore(num, size, false);
        } else {
            store = new RAMStore(num, size, true);
        }
    }
}

void ocall_nwrite_ramStore(size_t blockCount, long long* indexes, const char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t eachSize = len / blockCount;
    for (unsigned int i = 0; i < blockCount; i++) {
        block ciphertext(blk + (i * eachSize), blk + (i + 1) * eachSize);
        store->Write(indexes[i], ciphertext);
    }
}

void ocall_write_rawRamStore(long long index, const char *blk, size_t len) {
    size_t eachSize = len;
    block ciphertext(blk, blk + eachSize);
    store->WriteRawStore(index, ciphertext);
}

void ocall_nwrite_rawRamStore(size_t blockCount, long long* indexes, const char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t eachSize = len / blockCount;
    for (unsigned int i = 0; i < blockCount; i++) {
        block ciphertext(blk + (i * eachSize), blk + (i + 1) * eachSize);
        store->WriteRawStore(indexes[i], ciphertext);
    }
}

void ocall_nwrite_ramStore_by_client(vector<long long>* indexes, vector<block>* ciphertexts) {
    for (unsigned int i = 0; i < (*indexes).size(); i++) {
        store->Write((*indexes)[i], (*ciphertexts)[i]);
    }
}

void ocall_nwrite_raw_ramStore(vector<block>* ciphertexts) {
    for (unsigned int i = 0; i < (*ciphertexts).size(); i++) {
        store->WriteRawStore(i, (*ciphertexts)[i]);
    }
}

size_t ocall_nread_ramStore(size_t blockCount, long long* indexes, char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t resLen = -1;
    for (unsigned int i = 0; i < blockCount; i++) {
        block ciphertext = store->Read(indexes[i]);
        resLen = ciphertext.size();
        std::memcpy(blk + i * resLen, ciphertext.data(), ciphertext.size());
    }
    return resLen;
}

size_t ocall_read_rawRamStore(size_t index, char *blk, size_t len) {
    size_t resLen = -1;
    block ciphertext = store->ReadRawStore(index);
    resLen = ciphertext.size();
    std::memcpy(blk, ciphertext.data(), ciphertext.size());
    return resLen;
}

size_t ocall_nread_rawRamStore(size_t blockCount, size_t begin, char *blk, size_t len) {
    assert(len % blockCount == 0);
    size_t resLen = -1;
    size_t rawSize = store->tmpstore.size();
    for (unsigned int i = 0; i < blockCount && (begin + i) < rawSize; i++) {
        block ciphertext = store->ReadRawStore(i + begin);
        resLen = ciphertext.size();
        std::memcpy(blk + i * resLen, ciphertext.data(), ciphertext.size());
    }
    return resLen;
}

void ocall_initialize_ramStore(long long begin, long long end, const char *blk, size_t len) {
    block ciphertext(blk, blk + len);
    for (long long i = begin; i < end; i++) {
        store->Write(i, ciphertext);
    }
}

void ocall_write_ramStore(long long index, const char *blk, size_t len) {
    block ciphertext(blk, blk + len);
    store->Write(index, ciphertext);
}
#endif /* RAMSTOREENCLAVEINTERFACE_H */

