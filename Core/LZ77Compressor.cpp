#include "Core/LZ77Compressor.hpp"
#include "Utils/Logger.hpp"
#include <algorithm>
#include <cstring>

namespace compressor {

LZ77Compressor::HashChain::HashChain(size_t dataSize) {
    nodes_.reserve(dataSize / 2);
    head_.resize(HASH_SIZE, UINT32_MAX);
    data_.resize(dataSize);
}

uint32_t LZ77Compressor::HashChain::computeHash(const uint8_t* data, size_t pos) {
    return ((data[pos] << 14) ^ 
            (data[pos + 1] << 7) ^ 
            data[pos + 2]) & HASH_MASK;
}

void LZ77Compressor::HashChain::insert(uint32_t position) {
    if (position + 2 >= data_.size()) return;
    
    uint32_t hash = computeHash(data_.data(), position);
    nodes_.push_back({position, head_[hash]});
    head_[hash] = static_cast<uint32_t>(nodes_.size()) - 1;
}

std::vector<uint32_t> LZ77Compressor::HashChain::findMatches(
    uint32_t startPos, uint32_t maxLen) const {
    
    std::vector<uint32_t> matches;
    uint32_t hash = computeHash(data_.data(), startPos);
    
    for (uint32_t idx = head_[hash]; idx != UINT32_MAX; idx = nodes_[idx].next) {
        uint32_t matchPos = nodes_[idx].position;
        uint32_t offset = startPos - matchPos;
        
        if (offset > LZ77_WINDOW_SIZE) break;
        if (matchPos >= startPos) continue;
        
        uint32_t matchLen = 0;
        size_t limit = std::min(static_cast<size_t>(maxLen), data_.size() - startPos);
        
        while (matchLen < limit && 
               data_[matchPos + matchLen] == data_[startPos + matchLen]) {
            ++matchLen;
        }
        
        if (matchLen >= LZ77_MIN_MATCH) {
            matches.push_back(matchPos);
        }
    }
    
    return matches;
}

void LZ77Compressor::HashChain::clear() {
    std::fill(head_.begin(), head_.end(), UINT32_MAX);
    nodes_.clear();
}

LZ77Compressor::LZ77Compressor(size_t windowSize)
    : windowSize_(windowSize) {}

std::vector<LZ77Token> LZ77Compressor::compress(const std::vector<uint8_t>& data) {
    LOG_DEBUG("LZ77: Starting compression of " + std::to_string(data.size()) + " bytes");
    
    std::vector<LZ77Token> tokens;
    HashChain hashChain(data.size());
    std::memcpy(hashChain.data_.data(), data.data(), data.size());
    
    size_t pos = 0;
    size_t unprocessed = 0;
    
    while (pos < data.size()) {
        if (pos >= 3 && unprocessed == 0) {
            hashChain.insert(static_cast<uint32_t>(pos - 3));
        }
        
        size_t maxLen = std::min(LZ77_MAX_MATCH, 
                                 static_cast<size_t>(data.size() - pos));
        
        LZ77Token token = findLongestMatch(data, pos, maxLen, hashChain);
        tokens.push_back(token);
        
        if (token.type == LZ77Token::Type::Match) {
            unprocessed = token.length;
        } else {
            if (pos > 0) {
                hashChain.insert(static_cast<uint32_t>(pos));
            }
            ++pos;
        }
        
        while (unprocessed > 0 && pos < data.size()) {
            hashChain.insert(static_cast<uint32_t>(pos));
            ++pos;
            --unprocessed;
        }
    }
    
    LOG_DEBUG("LZ77: Compression complete, " + std::to_string(tokens.size()) + " tokens generated");
    compressedTokens_ = tokens;
    return tokens;
}

LZ77Token LZ77Compressor::findLongestMatch(
    const std::vector<uint8_t>& data,
    size_t startPos,
    size_t maxLen,
    HashChain& hashChain) {
    
    if (maxLen < LZ77_MIN_MATCH) {
        return LZ77Token::literalToken(data[startPos]);
    }

    auto candidates = hashChain.findMatches(
        static_cast<uint32_t>(startPos), 
        static_cast<uint32_t>(maxLen));
    
    if (candidates.empty()) {
        return LZ77Token::literalToken(data[startPos]);
    }
    
    size_t bestOffset = 0;
    size_t bestLength = 0;
    
    for (size_t matchPos : candidates) {
        size_t offset = startPos - matchPos;
        size_t matchLen = 0;
        
        while (matchLen < maxLen && data[matchPos + matchLen] == data[startPos + matchLen]) {
            ++matchLen;
        }
        
        if (matchLen > bestLength) {
            bestLength = matchLen;
            bestOffset = offset;
        }
    }
    
    if (bestLength >= LZ77_MIN_MATCH) {
        return LZ77Token::matchToken(
            static_cast<uint16_t>(std::min(bestOffset, static_cast<size_t>(UINT16_MAX))),
            static_cast<uint8_t>(std::min(bestLength, static_cast<size_t>(UINT8_MAX))));
    }
    
    return LZ77Token::literalToken(data[startPos]);
}

std::vector<uint8_t> LZ77Compressor::decompress(const std::vector<LZ77Token>& tokens) {
    LOG_DEBUG("LZ77: Starting decompression of " + std::to_string(tokens.size()) + " tokens");
    
    std::vector<uint8_t> output;
    output.reserve(tokens.size() * 2);
    
    for (const auto& token : tokens) {
        if (token.type == LZ77Token::Type::Literal) {
            output.push_back(token.literal);
        } else {
            size_t copyPos = output.size() - token.offset;
            for (size_t i = 0; i < token.length; ++i) {
                output.push_back(output[copyPos + i]);
            }
        }
    }
    
    LOG_DEBUG("LZ77: Decompression complete, " + std::to_string(output.size()) + " bytes");
    return output;
}

} // namespace compressor
