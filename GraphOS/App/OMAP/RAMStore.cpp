#include "RAMStore.hpp"

RAMStore::RAMStore(size_t count, bool simul)
: store(count), tmpstore(count), prfstore(count) {
    this->simulation = simul;
}

RAMStore::~RAMStore() {
}

block RAMStore::Read(long long pos) {
    if (simulation) {
        return store[0];
    } else {
        return store[pos];
    }
}

void RAMStore::Write(long long pos, block b) {
    if (!simulation) {
        store[pos] = b;
    } else {
        store[0] = b;
    }
}

void RAMStore::CreateRawStore(size_t count) {
    this->tmpstore.reserve(count);
}

void RAMStore::WriteRawStore(long long pos, block b) {
    tmpstore[pos] = b;
}

block RAMStore::ReadRawStore(long long pos) {
    return tmpstore[pos];
}

block RAMStore::ReadPRF(long long pos) {
    return prfstore[pos];
}

void RAMStore::WritePRF(long long pos, block b) {
    prfstore[pos] = b;
}
