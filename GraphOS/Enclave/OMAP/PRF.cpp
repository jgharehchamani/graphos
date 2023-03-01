#include <algorithm>
#include "PRF.h"
#include "Enclave.h"

PRF::PRF() {
    std::fill(id.begin(), id.end(), 0);
}

PRF::PRF(string value) {
    std::fill(id.begin(), id.end(), 0);
    std::copy(value.begin(), value.end(), id.begin());
}

PRF::PRF(long long value) {
    std::fill(id.begin(), id.end(), 0);
    for (int i = 0; i < 8; i++) {
        id[i] = (byte_t) (value >> (i * 8));
    }
    for (int i = 8; i < PRF_SIZE; i++) {
        id[i] = (byte_t) (0 >> (i * 8));
    }
}

PRF::PRF(std::array<byte_t, PRF_SIZE> value) {
    std::copy(value.begin(), value.end(), id.begin());
}

bool PRF::operator!=(const PRF rhs) const {
    return !CTeq(CTcmp(id, rhs), 0);
}

bool PRF::operator<(const PRF& rhs)const {
    return CTeq(CTcmp(id, rhs), -1);
}

bool PRF::operator<=(const PRF& rhs)const {
    return !CTeq(CTcmp(id, rhs), 1);
}

bool PRF::operator>(const PRF& rhs)const {
    return CTeq(CTcmp(id, rhs), 1);
}

bool PRF::operator>=(const PRF& rhs)const {
    return !CTeq(CTcmp(id, rhs), -1);
}

bool PRF::operator==(const PRF rhs) const {
    return CTeq(CTcmp(id, rhs), 0);
}

PRF& PRF::operator=(std::vector<byte_t> other) {
    for (int i = 0; i < PRF_SIZE; i++) {
        id[i] = other[i];
    }
    return *this;
}

PRF& PRF::operator=(PRF const &other) {
    for (int i = 0; i < PRF_SIZE; i++) {
        id[i] = other.id[i];
    }
    return *this;
}

long long PRF::getValue() {
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

void PRF::setValue(long long other) {
    std::fill(id.begin(), id.end(), 0);
    for (int i = 0; i < 8; i++) {
        id[i] = (byte_t) (other >> (i * 8));
    }
}

bool PRF::isZero() {
    int res = 0;
    bool found = false;
    for (int i = PRF_SIZE - 1; i >= 0; i--) {
        int cmpRes = CTcmp(id[i], 0);
        res = conditional_select(cmpRes, res, !found);
        found = conditional_select(true, found, !CTeq(cmpRes, 0) && !found);
    }
    return CTeq(res, 0);
}

void PRF::setToZero() {
    std::fill(id.begin(), id.end(), 0);
}


