#include "compressions.hpp"
#include <unordered_set>
#include <iostream>
#include <algorithm>
#include <queue>
#include <memory>

struct Node {
    std::shared_ptr<Node> child1;
    std::shared_ptr<Node> child2;

    u8 c;
    size_t count;
};

static std::shared_ptr<Node> createNode(const std::shared_ptr<Node>& n1, const std::shared_ptr<Node>& n2) {
    std::shared_ptr<Node> out = std::make_shared<Node>();
    out->child1 = n1;
    out->child2 = n2;
    out->c = 0;
    out->count = n1->count + n2->count;
    return out;
}

static void addU8(std::vector<bool>& vec, const u8& data) {
    vec.resize(vec.size() + 8);
    for (int i = 0; i < 8; i++) {
        vec[vec.size() - i - 1] = data & (1 << i);
    }
}

static void getCodes(std::vector<bool>* vecs, std::vector<bool>& bits, Node& n) {
    static std::vector<bool> code;

    // If the node is a leaf save the code
    if (n.child1 == nullptr) {
        vecs[n.c].insert(vecs[n.c].end(), code.begin(), code.end());
        bits.push_back(true);
        addU8(bits, n.c);
        return;
    }

    // It's a normal node
    bits.push_back(false);

    code.push_back(false);
    getCodes(vecs, bits, *n.child1);
    code.back() = true;
    getCodes(vecs, bits, *n.child2);
    code.pop_back();
}

std::vector<u8> huffman(size_t length, u8* data) {
    Node frequencies[256];
    for (u8 i = 0; i < 255; i++) {
        frequencies[i].c = i;
        frequencies[i].count = 0;
    }
    for (size_t i = 0; i < length; i++) {
        frequencies[data[i]].count++;
    }

    // add all the leafs into a priority queue so we don't manually have to sort
    auto cmp = [](const std::shared_ptr<Node>& n1, const std::shared_ptr<Node>& n2){ return n1->count > n2->count; };
    std::priority_queue<std::shared_ptr<Node>, std::vector<std::shared_ptr<Node>>, decltype(cmp)> pq;
    for (u8 i = 0; i < 255; i++) {
        if (frequencies[i].count) {
            std::shared_ptr<Node> buf = std::make_shared<Node>();
            buf->c = frequencies[i].c;
            buf->count = frequencies[i].count;
            buf->child1 = nullptr;
            buf->child2 = nullptr;
            pq.push(buf);
        }
    }

    // Create the tree
    while (pq.size() > 1) {
        std::shared_ptr<Node> n1 = pq.top();
        pq.pop();
        std::shared_ptr<Node> n2 = pq.top();
        pq.pop();
        pq.push(createNode(n1, n2));
    }

    // Get the root node
    std::shared_ptr<Node> root = pq.top();

    std::vector<bool> bits(3); // Initialized with size 3 to store the padding size
    std::vector<bool> codes[256];

    // Gets the codes for all chars and puts them in the codes vector. Also puts the entire tree into the bits vector
    getCodes(codes, bits, *root);

    for (auto b : bits) {
        printf("%i ", (int) b);
    }

    // Write the huffman codes according to the tree
    for (size_t i = 0; i < length; i++) {
        bits.insert(bits.end(), codes[data[i]].begin(), codes[data[i]].end());
    }

    u8 padSize = (8 - (bits.size() % 8)) % 8;

    // Pad the bits vector to have a size that's a multiple of 8
    for (int i = 0; i < padSize; i++) {
        bits.push_back(false);
    }

    // Write the padding size to the beginning of the binary data
    for (int i = 0; i < 3; i++) {
        bits[i] = padSize & (1 << (2 - i));
    }

    std::vector<u8> out;

    // Write the bits into the out vector
    for (size_t i = 0; i < bits.size(); i += 8) {
        u8 buffer = 0;
        for (int j = 0; j < 8; j++) {
            buffer |= bits[i + j] << (7 - j);
        }
        out.push_back(buffer);
    }

    printf("\n\n");

    return out;
}

static int treeSize;

static std::shared_ptr<Node> readTree(std::vector<bool>& bits) {
    // Stores the current recursion level. Used later to reset the readPos variable
    static int levels = 0;
    levels++;

    static int readPos = 3; // Set to 3 to skip the bits storing the padding size

    std::shared_ptr<Node> n = std::make_shared<Node>();

    if (!bits[readPos]) {
        readPos++;
        n->child1 = readTree(bits);
        readPos++;
        n->child2 = readTree(bits);
    } else {
        readPos++;
        u8 buffer = 0;
        for (int i = 0; i < 8; i++) {
            buffer |= bits[readPos + i] << (7 - i);
        }
        n->c = buffer;
        readPos += 7;
    }

    levels--;

    // Reset for later reuse
    if (!levels) {
        treeSize = readPos + 1;
        readPos = 3;
    }

    return n;
}

std::vector<u8> deHuffman(size_t length, u8* data) {
    std::vector<bool> bits(length*8);

    // Read bits into the bits vector
    for (size_t i = 0; i < length; i++) {
        for (int j = 0; j < 8; j++) {
            bits[i * 8 + (7 - j)] = data[i] & (1 << j);
        }
    }

    // Read padding size
    u8 paddingSize = 0;
    for (int i = 0; i < 3; i++) {
        paddingSize |= bits[i] << (2 - i);
    }

    std::shared_ptr<Node> root = readTree(bits);

    std::vector<u8> out;

    Node n = *root;

    // Read the huffman codes using the tree
    for (size_t i = treeSize; i < bits.size() - paddingSize; i++) {
        if (!bits[i]) {
            n = *n.child1;
        } else {
            n = *n.child2;
        }

        if (n.child1 == nullptr) {
            out.push_back(n.c);
            n = *root;
        }
    }

    return out;
};

std::vector<u8> LZ77(size_t length, u8* data, u8 offsetBits) {
    // Used to save possible starting positions
    std::unordered_set<size_t> startingPositions[256];
    std::vector<u8> out;

    if (offsetBits == 0 || offsetBits > 15) {
        return out;
    }

    for (size_t i = 0; i < length;) {
        u16 offBack = 0;
        u16 maxLen = 0;

        for (auto& pos : startingPositions[data[i]]) {
            if (i - pos > (1 << offsetBits) - 1) {
                // Delete starting positions that are too far away
                startingPositions[data[i]].erase(pos);
                continue;
            }

            // Counts number of bytes that we can copy
            u8 counter = 1;
            while (data[i + counter] == data[pos + counter] && counter < (1 << (16 - offsetBits)) - 1) {
                counter++;
            }

            if (maxLen <= counter) {
                maxLen = counter;
                offBack = i - pos;
            }

            if (maxLen > 1 << (16 - offsetBits)) {
                break;
            }
        }

        for (size_t j = i; j <= i + maxLen; j++) {
            // Insert all starting positions
            startingPositions[data[j]].insert(j);
        }

        i += maxLen + 1;

        out.push_back(offBack << std::max(8 - offsetBits, 0) | ((maxLen >> 8) & (0xff >> offsetBits)));
        out.push_back(((offBack >> (offsetBits - 8)) & (0xff << (16 - offsetBits))) | maxLen);
        out.push_back(i - 1 < length ? data[i - 1] : 0);
    }

    printf("\n");

    return out;
}

std::vector<u8> deLZ77(size_t length, u8 *data, u8 offsetBits) {
    std::vector<u8> out;

    if (offsetBits == 0 || offsetBits > 15) {
        return out;
    }

    for (size_t i = 0; i < length; i += 3) {
        u16 offBack = data[i] >> std::max(8 - offsetBits, 0);
        offBack += (data[i + 1] >> (16 - offsetBits)) << 8;
        u16 cpyLen = (data[i] & (0xff >> offsetBits)) << 8;
        cpyLen += data[i + 1] & (0xff >> std::max(offsetBits - 8, 0));
        u8 c = data[i + 2];

        out.resize(out.size() + cpyLen + 1);
        for (size_t j = 0; j < cpyLen; j++) {
            out[out.size() - 1 - cpyLen + j] = out[out.size() - 1 - cpyLen - offBack + j];
        }

        out[out.size() - 1] = c;
    }

    return out;
}

