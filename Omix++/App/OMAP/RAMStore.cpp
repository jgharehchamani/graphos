#include "RAMStore.hpp"

RAMStore::RAMStore(size_t count, size_t ram_size, bool simul)
: store(count), size(ram_size),tmpstore(count) {
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