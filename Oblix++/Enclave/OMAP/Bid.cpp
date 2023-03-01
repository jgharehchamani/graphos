#include <algorithm>
#include "Bid.h"
#include "Enclave.h"

Bid::Bid() {
    std::fill(id.begin(), id.end(), 0);
}

Bid::Bid(string value) {
    std::fill(id.begin(), id.end(), 0);
    std::copy(value.begin(), value.end(), id.begin());
}

Bid::Bid(long long value) {
    std::fill(id.begin(), id.end(), 0);
    for (int i = 0; i < 8; i++) {
        id[i] = (byte_t) (value >> (i * 8));
    }
    for (int i = 8; i < ID_SIZE; i++) {
        id[i] = (byte_t) (0 >> (i * 8));
    }
}

Bid::Bid(std::array<byte_t, ID_SIZE> value) {
    std::copy(value.begin(), value.end(), id.begin());
}

/**
 * constant time comparator
 * @param left
 * @param right
 * @return left < right -> -1,  left = right -> 0, left > right -> 1
 */
signed char Bid::CTcmp(byte_t lhs, byte_t rhs) {
    unsigned short overflowing_iff_lt = (short) lhs - (short) rhs;
    unsigned short overflowing_iff_gt = (short) rhs - (short) lhs;
    signed char is_less_than = (signed char) -(overflowing_iff_lt >> 15); // -1 if self < other, 0 otherwise.
    signed char is_greater_than = (signed char) (overflowing_iff_gt >> 15); // 1 if self > other, 0 otherwise.
    signed char result = is_less_than + is_greater_than;
    return result;
}

bool Bid::operator!=(const Bid rhs) const {
    bool res = true;
    for (int i = ID_SIZE - 1; i >= 0; i--) {

        if (id[i]^rhs.id[i]) {
            res = false;
        } else {
            res = res;
        }
    }
    return !res;
}

bool Bid::operator<(const Bid& b)const {
    bool isIdentified = false;
    int res = 0;
    for (int i = ID_SIZE - 1; i >= 0; i--) {
        if (!isIdentified && !(Bid::CTcmp(id[i], b.id[i])^((signed char) - 1))) {
            res = 1;
            isIdentified = true;
        } else if (!isIdentified && !(Bid::CTcmp(id[i], b.id[i])^((signed char) 1))) {
            res = -1;
            isIdentified = true;
        } else {
            res = res;
            isIdentified = isIdentified;
        }
    }
    return res^1 ? false : true;
}

bool Bid::operator<=(const Bid& b)const {
    bool isIdentified = false;
    int res = 0;

    for (int i = ID_SIZE - 1; i >= 0; i--) {
        if (!isIdentified && !(Bid::CTcmp(id[i], b.id[i])^((signed char) - 1))) {
            res = 1;
            isIdentified = true;
        } else if (!isIdentified && !(Bid::CTcmp(id[i], b.id[i])^((signed char) 1))) {
            res = -1;
            isIdentified = true;
        } else {
            res = res;
            isIdentified = isIdentified;
        }
    }
    return res^-1 ? true : false;
}

bool Bid::operator>(const Bid& b)const {
    bool isIdentified = false;
    int res = 0;


    for (int i = ID_SIZE - 1; i >= 0; i--) {
        if (!isIdentified && !(Bid::CTcmp(id[i], b.id[i])^((signed char) 1))) {
            res = 1;
            isIdentified = true;
        } else if (!isIdentified && !(Bid::CTcmp(id[i], b.id[i])^((signed char) - 1))) {
            res = -1;
            isIdentified = true;
        } else {
            res = res;
            isIdentified = isIdentified;
        }
    }
    return res^1 ? false : true;
}

bool Bid::operator>=(const Bid& b)const {
    bool isIdentified = false;
    int res = 0;

    for (int i = ID_SIZE - 1; i >= 0; i--) {
        if (!isIdentified && !(Bid::CTcmp(id[i], b.id[i])^((signed char) 1))) {
            res = 1;
            isIdentified = true;
        } else if (!isIdentified && !(Bid::CTcmp(id[i], b.id[i])^((signed char) - 1))) {
            res = -1;
            isIdentified = true;
        } else {
            res = res;
            isIdentified = isIdentified;
        }
    }
    return res^-1 ? true : false;
}

bool Bid::operator==(const Bid rhs) const {
    bool res = true;
    for (int i = 0; i < ID_SIZE; i++) {
        if (id[i]^rhs.id[i]) {
            res = false;
        } else {
            res = res;
        }
    }
    return res;
}

Bid& Bid::operator=(std::vector<byte_t> other) {
    for (int i = 0; i < ID_SIZE; i++) {
        id[i] = other[i];
    }
    return *this;
}

Bid& Bid::operator=(Bid const &other) {
    for (int i = 0; i < ID_SIZE; i++) {
        id[i] = other.id[i];
    }
    return *this;
}

long long Bid::getValue() {
    long long result = 0;
    result += id[0];
    result += (id[1] << 8);
    result += (id[2] << 16);
    result += (id[3] << 24);
    result += (id[4] << 24);
    result += (id[5] << 32);
    result += (id[6] << 40);
    result += (id[7] << 48);
    return result;
}

void Bid::setValue(long long other) {
    std::fill(id.begin(), id.end(), 0);
    for (int i = 0; i < 8; i++) {
        id[i] = (byte_t) (other >> (i * 8));
    }
}
