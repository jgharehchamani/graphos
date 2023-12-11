#pragma once
#include <map>
#include <array>
#include <vector>

using namespace std;

using byte_t = uint8_t;
using block = std::vector<byte_t>;

class LocalRAMStore {
    std::vector<block> store;
    size_t size;

public:
    LocalRAMStore(size_t num, size_t size);
    ~LocalRAMStore();

    block Read(long long pos);
    void Write(long long pos, block b);

};
