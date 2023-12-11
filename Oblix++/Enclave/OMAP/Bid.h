#ifndef BID_H
#define BID_H
#include "Types.hpp"
#include <array>
#include <string>
using namespace std;

class Bid {
public:
    std::array< byte_t, ID_SIZE> id;
    Bid();
    Bid(long long value);
    Bid(std::array< byte_t, ID_SIZE> value);
    Bid(string value);
    bool operator!=(const Bid rhs) const;
    bool operator==(const Bid rhs) const;
    Bid& operator=(std::vector<byte_t> other);
    Bid& operator=(Bid const &other);
    bool operator<(const Bid& b) const;
    bool operator>(const Bid& b) const;
    bool operator<=(const Bid& b) const;
    bool operator>=(const Bid& b) const;
    long long getValue();
    void setValue(long long value);
    static signed char CTcmp(byte_t lhs, byte_t rhs);
};



#endif /* BID_H */
