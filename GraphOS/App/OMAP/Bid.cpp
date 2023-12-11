#include <algorithm>
#include "Bid.h"

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

bool Bid::operator!=(const Bid rhs) const {
    return !CTeq(CTcmp(id, rhs), 0);
}

bool Bid::operator<(const Bid& rhs)const {
    return CTeq(CTcmp(id, rhs), -1);
}

bool Bid::operator<=(const Bid& rhs)const {
    return !CTeq(CTcmp(id, rhs), 1);
}

bool Bid::operator>(const Bid& rhs)const {
    return CTeq(CTcmp(id, rhs), 1);
}

bool Bid::operator>=(const Bid& rhs)const {
    return !CTeq(CTcmp(id, rhs), -1);
}

bool Bid::operator==(const Bid rhs) const {
    return CTeq(CTcmp(id, rhs), 0);
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

bool Bid::isZero() {
    int res = 0;
    bool found = false;
    for (int i = ID_SIZE - 1; i >= 0; i--) {
        int cmpRes = CTcmp(id[i], 0);
        res = conditional_select(cmpRes, res, !found);
        found = conditional_select(true, found, !CTeq(cmpRes, 0) && !found);
    }
    return CTeq(res, 0);
}

void Bid::setToZero() {
    std::fill(id.begin(), id.end(), 0);
}


