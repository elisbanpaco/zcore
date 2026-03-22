#ifndef CORE_COMPRESSIONENGINE_HPP
#define CORE_COMPRESSIONENGINE_HPP

#include "Core/LZ77Compressor.hpp"
#include "Core/HuffmanCompressor.hpp"
#include "Utils/CompressionException.hpp"
#include "Utils/Logger.hpp"
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace compressor {

constexpr uint32_t MAGIC_NUMBER = 0x48445043;
constexpr uint8_t VERSION_MAJOR = 1;
constexpr uint8_t VERSION_MINOR = 0;

struct CompressionHeader {
    uint32_t magic;
    uint8_t majorVersion;
    uint8_t minorVersion;
    uint32_t originalSize;
    uint32_t compressedSize;
    uint32_t lz77TokenCount;
    uint32_t symbolDataSize;
};

class CompressionEngine {
public:
    CompressionEngine();
    ~CompressionEngine() = default;

    void compress(const std::string& inputPath, const std::string& outputPath);
    void decompress(const std::string& inputPath, const std::string& outputPath);

    double getCompressionRatio() const;
    size_t getOriginalSize() const { return originalSize_; }
    size_t getCompressedSize() const { return compressedSize_; }

private:
    void writeCompressedFile(const std::string& path,
                             const std::vector<LZ77Token>& lz77Tokens,
                             const std::vector<uint8_t>& literalData,
                             size_t originalSize);

    std::vector<LZ77Token> readCompressedTokens(std::ifstream& stream,
                                                const CompressionHeader& header);

    std::unique_ptr<LZ77Compressor> lz77_;
    std::unique_ptr<HuffmanCompressor> huffman_;
    
    size_t originalSize_;
    size_t compressedSize_;
};

} // namespace compressor

#endif // CORE_COMPRESSIONENGINE_HPP
