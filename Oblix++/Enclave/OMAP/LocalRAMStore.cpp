#include "LocalRAMStore.hpp"

LocalRAMStore::LocalRAMStore(size_t count, size_t ram_size)
: store(count), size(ram_size) {
}

LocalRAMStore::~LocalRAMStore() {
}

block LocalRAMStore::Read(long long pos) {
    return store[pos];
}

void LocalRAMStore::Write(long long pos, block b) {
    store[pos] = b;
}
