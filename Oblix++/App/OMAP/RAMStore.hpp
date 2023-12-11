#pragma once
#include <map>
#include <array>
#include <vector>

using namespace std;

using byte_t = uint8_t;
using block = std::vector<byte_t>;

class RAMStore {
    std::vector<block> store;
    size_t size;
    bool simulation;

public:
    RAMStore(size_t num, size_t size, bool simulation);
    ~RAMStore();

    block Read(long long pos);
    void Write(long long pos, block b);

};
