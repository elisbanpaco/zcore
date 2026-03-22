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
constexpr uint8_t VERSION_MAJOR = 2;
constexpr uint8_t VERSION_MINOR = 0;
constexpr uint32_t DEFAULT_BLOCK_SIZE = 64 * 1024;

static constexpr uint8_t BLOCK_FLAG_STORED    = 0x00;
static constexpr uint8_t BLOCK_FLAG_COMPRESSED = 0x01;

#pragma pack(push, 1)
struct CompressionHeader {
    uint32_t magic;
    uint8_t majorVersion;
    uint8_t minorVersion;
    uint32_t originalSize;
    uint32_t blockSize;
    uint32_t blockCount;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BlockHeader {
    uint8_t flags;
    uint32_t dataSize;
};
#pragma pack(pop)

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
    void compressAndWriteBlock(std::ofstream& output,
                               const std::vector<uint8_t>& data,
                               size_t& totalCompressedSize);

    void readAndDecompressBlock(std::ifstream& input,
                                const BlockHeader& bh,
                                std::vector<uint8_t>& output);

    std::unique_ptr<LZ77Compressor> lz77_;
    std::unique_ptr<HuffmanCompressor> huffman_;

    size_t originalSize_;
    size_t compressedSize_;
};

} // namespace compressor

#endif // CORE_COMPRESSIONENGINE_HPP
