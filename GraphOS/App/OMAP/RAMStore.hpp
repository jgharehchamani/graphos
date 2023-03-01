#pragma once
#include <map>
#include <array>
#include <vector>

using namespace std;

using byte_t = uint8_t;
using block = std::vector<byte_t>;

class RAMStore {
    std::vector<block> store;
    bool simulation;

public:
    RAMStore(size_t num, bool simulation);
    ~RAMStore();
    std::vector<block> tmpstore;
    std::vector<block> prfstore;

    block Read(long long pos);
    void Write(long long pos, block b);
    block ReadPRF(long long pos);
    void WritePRF(long long pos, block b);
    void CreateRawStore(size_t count);
    void WriteRawStore(long long pos, block b);
    block ReadRawStore(long long pos);

};
