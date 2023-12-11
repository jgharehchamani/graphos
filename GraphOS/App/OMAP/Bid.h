#ifndef BID_H
#define BID_H
#include <array>
#include <string>
#include "AES.hpp"
using namespace std;

class Bid {
public:

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> a , choice = 0 -> return b
     */
    static int conditional_select(int a, int b, int choice) {
        unsigned int one = 1;
        return (~((unsigned int) choice - one) & a) | ((unsigned int) (choice - one) & b);
    }

    static bool CTeq(int a, int b) {
        return !(a^b);
    }

    static bool CTeq(long long a, long long b) {
        return !(a^b);
    }

    static bool CTeq(unsigned __int128 a, unsigned __int128 b) {
        return !(a^b);
    }

    static bool CTeq(unsigned long long a, unsigned long long b) {
        return !(a^b);
    }

    static int CTcmp(long long lhs, long long rhs) {
        unsigned __int128 overflowing_iff_lt = (__int128) lhs - (__int128) rhs;
        unsigned __int128 overflowing_iff_gt = (__int128) rhs - (__int128) lhs;
        int is_less_than = (int) -(overflowing_iff_lt >> 127); // -1 if self < other, 0 otherwise.
        int is_greater_than = (int) (overflowing_iff_gt >> 127); // 1 if self > other, 0 otherwise.
        int result = is_less_than + is_greater_than;
        return result;
    }

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> a , choice = 0 -> return b
     */
    static long long conditional_select(long long a, long long b, int choice) {
        unsigned long long one = 1;
        return (~((unsigned long long) choice - one) & a) | ((unsigned long long) (choice - one) & b);
    }

    static unsigned long long conditional_select(unsigned long long a, unsigned long long b, int choice) {
        unsigned long long one = 1;
        return (~((unsigned long long) choice - one) & a) | ((unsigned long long) (choice - one) & b);
    }

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> a , choice = 0 -> return b
     */
    static unsigned __int128 conditional_select(unsigned __int128 a, unsigned __int128 b, int choice) {
        unsigned __int128 one = 1;
        return (~((unsigned __int128) choice - one) & a) | ((unsigned __int128) (choice - one) & b);
    }

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> a , choice = 0 -> return b
     */
    static byte_t conditional_select(byte_t a, byte_t b, int choice) {
        byte_t one = 1;
        return (~((byte_t) choice - one) & a) | ((byte_t) (choice - one) & b);
    }

    /**
     * constant time comparator
     * @param left
     * @param right
     * @return left < right -> -1,  left = right -> 0, left > right -> 1
     */
    static signed char CTcmp(byte_t lhs, byte_t rhs) {
        unsigned short overflowing_iff_lt = (short) lhs - (short) rhs;
        unsigned short overflowing_iff_gt = (short) rhs - (short) lhs;
        signed char is_less_than = (signed char) -(overflowing_iff_lt >> 15); // -1 if self < other, 0 otherwise.
        signed char is_greater_than = (signed char) (overflowing_iff_gt >> 15); // 1 if self > other, 0 otherwise.
        signed char result = is_less_than + is_greater_than;
        return result;
    }

    static int CTcmp(Bid lhs, Bid rhs) {
        int res = 0;
        bool found = false;
        for (int i = ID_SIZE - 1; i >= 0; i--) {
            int cmpRes = CTcmp(lhs.id[i], rhs.id[i]);
            res = conditional_select(cmpRes, res, !found);
            found = conditional_select(true, found, !CTeq(cmpRes, 0) && !found);
        }
        return res;
    }

    /**
     * constant time selector
     * @param a
     * @param b
     * @param choice 0 or 1
     * @return choice = 1 -> a , choice = 0 -> return b
     */
    static Bid conditional_select(Bid a, Bid b, int choice) {
        Bid res;
        for (int i = 0; i < res.id.size(); i++) {
            res.id[i] = Bid::conditional_select(a.id[i], b.id[i], choice);
        }
        return res;
    }

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
    void setToZero();
    bool isZero();
    void setValue(long long value);
};



#endif /* BID_H */
