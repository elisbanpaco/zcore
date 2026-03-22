#ifndef CORE_LZ77COMPRESSOR_HPP
#define CORE_LZ77COMPRESSOR_HPP

#include <cstdint>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace compressor {

constexpr size_t LZ77_WINDOW_SIZE = 32 * 1024;
constexpr size_t LZ77_LOOKAHEAD_SIZE = 258;
constexpr size_t LZ77_MIN_MATCH = 3;
constexpr size_t LZ77_MAX_MATCH = 258;

struct LZ77Token {
    enum class Type { Literal, Match } type;
    uint8_t literal;
    uint16_t offset;
    uint8_t length;

    static LZ77Token literalToken(uint8_t byte) {
        return {Type::Literal, byte, 0, 0};
    }

    static LZ77Token matchToken(uint16_t offset, uint8_t length) {
        return {Type::Match, 0, offset, length};
    }
};

class LZ77Compressor {
public:
    LZ77Compressor(size_t windowSize = LZ77_WINDOW_SIZE);
    ~LZ77Compressor() = default;

    std::vector<LZ77Token> compress(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decompress(const std::vector<LZ77Token>& tokens);

    size_t getCompressedSize() const { return compressedTokens_.size(); }

private:
    struct HashChain {
        static constexpr size_t HASH_BITS = 15;
        static constexpr size_t HASH_SIZE = 1 << HASH_BITS;
        static constexpr size_t HASH_MASK = HASH_SIZE - 1;

        struct Node {
            uint32_t position;
            uint32_t next;
        };

        std::vector<Node> nodes_;
        std::vector<uint32_t> head_;
        std::vector<uint8_t> data_;

        explicit HashChain(size_t dataSize);

        void insert(uint32_t position);
        std::vector<uint32_t> findMatches(uint32_t startPos, uint32_t maxLen) const;
        void clear();

    private:
        static uint32_t computeHash(const uint8_t* data, size_t pos);
    };

    LZ77Token findLongestMatch(const std::vector<uint8_t>& data, 
                                size_t startPos, 
                                size_t maxLen,
                                HashChain& hashChain);
    
    size_t windowSize_;
    std::vector<LZ77Token> compressedTokens_;
};

} // namespace compressor

#endif // CORE_LZ77COMPRESSOR_HPP
