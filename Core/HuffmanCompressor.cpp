#include "Core/HuffmanCompressor.hpp"
#include "Utils/Logger.hpp"
#include <algorithm>
#include <cstring>

namespace compressor {

struct FrequencyComparator {
    bool operator()(HuffmanNode* a, HuffmanNode* b) const {
        return a->frequency > b->frequency;
    }
};

HuffmanNode::HuffmanNode(uint16_t sym, uint32_t freq)
    : symbol(sym), frequency(freq), left(nullptr), right(nullptr), isLeaf(true) {}

HuffmanNode::HuffmanNode(HuffmanNode* l, HuffmanNode* r)
    : symbol(0), frequency(l->frequency + r->frequency), left(l), right(r), isLeaf(false) {}

HuffmanNode::~HuffmanNode() {
    if (left) delete left;
    if (right) delete right;
}

HuffmanCompressor::HuffmanCompressor() : root_(nullptr) {}

HuffmanCompressor::~HuffmanCompressor() {
    deleteTree(root_);
}

void HuffmanCompressor::deleteTree(HuffmanNode* node) {
    if (!node) return;
    if (node->isLeaf) {
        delete node;
        return;
    }
    deleteTree(node->left);
    deleteTree(node->right);
    delete node;
}

void HuffmanCompressor::buildTree(const std::vector<uint8_t>& data) {
    frequencies_.clear();
    countFrequencies(data, frequencies_);
    buildTreeFromFrequencies(frequencies_);
}

void HuffmanCompressor::buildTreeFromFrequencies(
    const std::unordered_map<uint16_t, uint32_t>& frequencies) {
    
    deleteTree(root_);
    root_ = nullptr;
    codes_.clear();
    symbolList_.clear();

    if (frequencies.empty()) return;

    std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, FrequencyComparator> pq;

    for (const auto& [sym, freq] : frequencies) {
        pq.push(new HuffmanNode(sym, freq));
        symbolList_.push_back(sym);
    }

    while (pq.size() > 1) {
        HuffmanNode* left = pq.top(); pq.pop();
        HuffmanNode* right = pq.top(); pq.pop();
        pq.push(new HuffmanNode(left, right));
    }

    root_ = pq.empty() ? nullptr : pq.top();
    
    if (root_ && root_->isLeaf) {
        codes_[root_->symbol] = {{false}, 1};
    } else if (root_) {
        std::vector<bool> prefix;
        generateCodes(root_, prefix);
        buildCanonicalCodes();
    }
}

void HuffmanCompressor::generateCodes(HuffmanNode* node, const std::vector<bool>& prefix) {
    if (!node) return;

    if (node->isLeaf) {
        codes_[node->symbol] = {prefix, static_cast<uint8_t>(prefix.size())};
        return;
    }

    std::vector<bool> leftPrefix = prefix;
    leftPrefix.push_back(false);
    generateCodes(node->left, leftPrefix);

    std::vector<bool> rightPrefix = prefix;
    rightPrefix.push_back(true);
    generateCodes(node->right, rightPrefix);
}

void HuffmanCompressor::buildCanonicalCodes() {
    std::map<uint8_t, std::vector<uint16_t>> lengths;
    size_t maxLength = 0;

    for (const auto& [sym, code] : codes_) {
        lengths[code.length].push_back(sym);
        maxLength = std::max(maxLength, static_cast<size_t>(code.length));
    }

    std::vector<uint16_t> nextCode(maxLength + 1, 0);
    for (size_t len = 1; len <= maxLength; ++len) {
        nextCode[len] = (nextCode[len - 1] + 
                        static_cast<uint16_t>(lengths[len - 1].size())) << 1;
    }

    for (size_t len = 1; len <= maxLength; ++len) {
        for (uint16_t sym : lengths[len]) {
            codes_[sym] = {std::vector<bool>(), static_cast<uint8_t>(len)};
            uint16_t code = nextCode[len];
            for (int i = static_cast<int>(len) - 1; i >= 0; --i) {
                codes_[sym].bits.push_back((code >> i) & 1);
            }
            nextCode[len]++;
        }
    }
}

void HuffmanCompressor::countFrequencies(
    const std::vector<uint8_t>& data,
    std::unordered_map<uint16_t, uint32_t>& frequencies) {
    
    for (uint8_t byte : data) {
        frequencies[byte]++;
    }
}

std::vector<uint8_t> HuffmanCompressor::compress(const std::vector<uint8_t>& data) {
    LOG_DEBUG("Huffman: Building tree for " + std::to_string(data.size()) + " bytes");
    
    buildTree(data);
    
    std::vector<uint8_t> header;
    serializeTree(root_, header);
    
    size_t headerSize = header.size();
    std::vector<uint8_t> output;
    output.insert(output.end(), 
                  reinterpret_cast<uint8_t*>(&headerSize), 
                  reinterpret_cast<uint8_t*>(&headerSize) + sizeof(headerSize));
    output.insert(output.end(), header.begin(), header.end());
    
    uint8_t buffer = 0;
    uint8_t bitPos = 0;
    
    for (uint8_t byte : data) {
        const auto& code = codes_[byte];
        for (bool bit : code.bits) {
            if (bit) buffer |= (1 << bitPos);
            ++bitPos;
            if (bitPos == 8) {
                output.push_back(buffer);
                buffer = 0;
                bitPos = 0;
            }
        }
    }
    
    if (bitPos > 0) {
        output.push_back(buffer);
    }
    
    LOG_DEBUG("Huffman: Compressed to " + std::to_string(output.size()) + " bytes");
    return output;
}

std::vector<uint8_t> HuffmanCompressor::decompress(const std::vector<uint8_t>& data, 
                                                     size_t originalSize) {
    size_t pos = 0;
    
    size_t headerSize;
    std::memcpy(&headerSize, data.data() + pos, sizeof(headerSize));
    pos += sizeof(headerSize);
    
    std::vector<uint8_t> header(data.begin() + pos, data.begin() + pos + headerSize);
    pos += headerSize;
    
    size_t headerPos = 0;
    root_ = deserializeTree(header, headerPos);
    
    std::vector<uint8_t> output;
    output.reserve(originalSize);
    
    HuffmanNode* current = root_;
    
    while (output.size() < originalSize && pos < data.size()) {
        uint8_t byte = data[pos++];
        for (int i = 0; i < 8 && output.size() < originalSize; ++i) {
            current = (byte & (1 << i)) ? current->right : current->left;
            
            if (current->isLeaf) {
                output.push_back(static_cast<uint8_t>(current->symbol));
                current = root_;
            }
        }
    }
    
    return output;
}

void HuffmanCompressor::serializeTree(HuffmanNode* node, std::vector<uint8_t>& output) const {
    if (!node) return;
    
    if (node->isLeaf) {
        output.push_back(1);
        output.push_back(static_cast<uint8_t>(node->symbol));
    } else {
        output.push_back(0);
        serializeTree(node->left, output);
        serializeTree(node->right, output);
    }
}

HuffmanNode* HuffmanCompressor::deserializeTree(const std::vector<uint8_t>& input, size_t& pos) {
    if (pos >= input.size()) return nullptr;
    
    if (input[pos++] == 1) {
        return new HuffmanNode(input[pos++], 1);
    }
    
    HuffmanNode* left = deserializeTree(input, pos);
    HuffmanNode* right = deserializeTree(input, pos);
    return new HuffmanNode(left, right);
}

std::vector<uint8_t> HuffmanCompressor::compressSymbols(const std::vector<uint16_t>& symbols) {
    std::unordered_map<uint16_t, uint32_t> freqMap;
    for (uint16_t s : symbols) {
        freqMap[s]++;
    }
    
    buildTreeFromFrequencies(freqMap);
    
    std::vector<uint8_t> header;
    serializeTree(root_, header);
    
    size_t headerSize = header.size();
    std::vector<uint8_t> output;
    output.insert(output.end(),
                  reinterpret_cast<uint8_t*>(&headerSize),
                  reinterpret_cast<uint8_t*>(&headerSize) + sizeof(headerSize));
    output.insert(output.end(), header.begin(), header.end());
    
    uint32_t count = static_cast<uint32_t>(symbols.size());
    output.insert(output.end(),
                  reinterpret_cast<uint8_t*>(&count),
                  reinterpret_cast<uint8_t*>(&count) + sizeof(count));
    
    uint8_t buffer = 0;
    uint8_t bitPos = 0;
    
    for (uint16_t sym : symbols) {
        if (codes_.find(sym) != codes_.end()) {
            for (bool bit : codes_[sym].bits) {
                if (bit) buffer |= (1 << bitPos);
                ++bitPos;
                if (bitPos == 8) {
                    output.push_back(buffer);
                    buffer = 0;
                    bitPos = 0;
                }
            }
        }
    }
    
    if (bitPos > 0) {
        output.push_back(buffer);
    }
    
    return output;
}

std::vector<uint8_t> HuffmanCompressor::decompressSymbols(const std::vector<uint8_t>& compressed,
                                                            size_t symbolCount) {
    size_t pos = 0;
    
    size_t headerSize;
    std::memcpy(&headerSize, compressed.data() + pos, sizeof(headerSize));
    pos += sizeof(headerSize);
    
    std::vector<uint8_t> header(compressed.begin() + pos, compressed.begin() + pos + headerSize);
    pos += headerSize;
    
    uint32_t count;
    std::memcpy(&count, compressed.data() + pos, sizeof(count));
    pos += sizeof(count);
    
    size_t headerPos = 0;
    root_ = deserializeTree(header, headerPos);
    
    std::vector<uint8_t> output;
    HuffmanNode* current = root_;
    
    while (output.size() < symbolCount && pos < compressed.size()) {
        uint8_t byte = compressed[pos++];
        for (int i = 0; i < 8 && output.size() < symbolCount; ++i) {
            current = (byte & (1 << i)) ? current->right : current->left;
            
            if (current->isLeaf) {
                output.push_back(static_cast<uint8_t>(current->symbol));
                current = root_;
            }
        }
    }
    
    return output;
}

} // namespace compressor
