#ifndef CORE_HUFFMANCOMPRESSOR_HPP
#define CORE_HUFFMANCOMPRESSOR_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace compressor {

struct HuffmanNode {
    uint16_t symbol;
    uint32_t frequency;
    HuffmanNode* left;
    HuffmanNode* right;
    bool isLeaf;

    HuffmanNode(uint16_t sym, uint32_t freq);
    HuffmanNode(HuffmanNode* l, HuffmanNode* r);
    ~HuffmanNode();
};

struct HuffmanCode {
    std::vector<bool> bits;
    uint8_t length;
};

class HuffmanCompressor {
public:
    HuffmanCompressor();
    ~HuffmanCompressor();

    std::vector<uint8_t> compress(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data, size_t originalSize);

    std::vector<uint8_t> compressSymbols(const std::vector<uint16_t>& symbols);
    std::vector<uint8_t> decompressSymbols(const std::vector<uint8_t>& compressed, 
                                           size_t symbolCount);

    void buildTree(const std::vector<uint8_t>& data);
    void buildTreeFromFrequencies(const std::unordered_map<uint16_t, uint32_t>& frequencies);

private:
    void generateCodes(HuffmanNode* node, const std::vector<bool>& prefix);
    void buildCanonicalCodes();
    void deleteTree(HuffmanNode* node);
    
    void serializeTree(HuffmanNode* node, std::vector<uint8_t>& output) const;
    HuffmanNode* deserializeTree(const std::vector<uint8_t>& input, size_t& pos);

    static void countFrequencies(const std::vector<uint8_t>& data,
                                 std::unordered_map<uint16_t, uint32_t>& frequencies);

    HuffmanNode* root_;
    std::unordered_map<uint16_t, HuffmanCode> codes_;
    std::unordered_map<uint16_t, uint32_t> frequencies_;
    std::vector<uint16_t> symbolList_;
};

} // namespace compressor

#endif // CORE_HUFFMANCOMPRESSOR_HPP
