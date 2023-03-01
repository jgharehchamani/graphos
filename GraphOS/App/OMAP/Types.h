/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   Types.h
 * Author: jgc
 *
 * Created on April 19, 2021, 12:45 PM
 */

#ifndef TYPES_H
#define TYPES_H

#include <array>
#include <vector>
#include <iostream>
#include <cstdint>

#define ID_SIZE 16

using byte_t = uint8_t;
using block = std::vector<byte_t>;

template <size_t N>
using bytes = std::array<byte_t, N>;
constexpr int Z = 4;

struct Block {
    unsigned long long id;
    block data;
};

using Bucket = std::array<Block, Z>;

// Encryption constants (in bytes)
constexpr int IV = 16;
constexpr int Key = 128;

template< typename T >
std::array< byte_t, sizeof (T) > to_bytes(const T& object) {
    std::array< byte_t, sizeof (T) > bytes;

    const byte_t* begin = reinterpret_cast<const byte_t*> (std::addressof(object));
    const byte_t* end = begin + sizeof (T);
    std::copy(begin, end, std::begin(bytes));

    return bytes;
}

template< typename T >
T& from_bytes(const std::array< byte_t, sizeof (T) >& bytes, T& object) {
    byte_t* begin_object = reinterpret_cast<byte_t*> (std::addressof(object));
    std::copy(std::begin(bytes), std::end(bytes), begin_object);

    return object;
}

#endif /* TYPES_H */

