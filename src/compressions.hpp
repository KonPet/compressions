#pragma once
#include <vector>
#include "types.hpp"

std::vector<u8> huffman(size_t length, u8* data);
std::vector<u8> deHuffman(size_t length, u8* data);

std::vector<u8> LZ77(size_t length, u8* data, u8 offsetBits = 8);
std::vector<u8> deLZ77(size_t length, u8* data, u8 offsetBits = 8);

std::vector<u8> LZSS(size_t length, u8 *data, u8 offsetBits = 8);
std::vector<u8> deLZSS(size_t length, u8 *data, u8 offsetBits = 8);
