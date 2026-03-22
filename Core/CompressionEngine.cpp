#include "Core/CompressionEngine.hpp"
#include "IO/BitBuffer.hpp"
#include "IO/FileHandler.hpp"
#include <algorithm>
#include <cstring>
#include <sstream>

namespace compressor {

CompressionEngine::CompressionEngine()
    : lz77_(std::make_unique<LZ77Compressor>()),
      huffman_(std::make_unique<HuffmanCompressor>()),
      originalSize_(0),
      compressedSize_(0) {}

void CompressionEngine::compress(const std::string& inputPath, const std::string& outputPath) {
    LOG_INFO("Starting compression (block-level store fallback): " + inputPath + " -> " + outputPath);

    try {
        FileHandler input(inputPath, false);
        originalSize_ = input.getFileSize();

        std::ofstream output(outputPath, std::ios::binary);
        if (!output.is_open()) {
            throw FileOpenError(outputPath, true);
        }

        CompressionHeader header = {
            MAGIC_NUMBER,
            VERSION_MAJOR,
            VERSION_MINOR,
            static_cast<uint32_t>(originalSize_),
            DEFAULT_BLOCK_SIZE,
            0
        };

        output.write(reinterpret_cast<const char*>(&header), sizeof(header));

        std::vector<uint8_t> chunk;
        uint32_t blockCount = 0;
        size_t totalCompressedSize = sizeof(CompressionHeader);

        while (input.readChunk(chunk) > 0) {
            compressAndWriteBlock(output, chunk, totalCompressedSize);
            ++blockCount;

            LOG_DEBUG("Block " + std::to_string(blockCount) +
                      " (" + std::to_string(chunk.size()) + " bytes)");
        }

        output.seekp(offsetof(CompressionHeader, blockCount));
        output.write(reinterpret_cast<const char*>(&blockCount), sizeof(blockCount));

        compressedSize_ = totalCompressedSize;
        output.close();

        LOG_INFO("Compression complete: " + std::to_string(blockCount) + " blocks, " +
                 std::to_string(originalSize_) + " -> " + std::to_string(compressedSize_) +
                 " bytes (" + std::to_string(getCompressionRatio()) + "% reduction)");

    } catch (const CompressionError& e) {
        LOG_ERROR(std::string("Compression failed: ") + e.what());
        throw;
    }
}

void CompressionEngine::decompress(const std::string& inputPath, const std::string& outputPath) {
    LOG_INFO("Starting decompression: " + inputPath + " -> " + outputPath);

    try {
        std::ifstream input(inputPath, std::ios::binary);
        if (!input.is_open()) {
            throw FileOpenError(inputPath, false);
        }

        CompressionHeader header;
        input.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (header.magic != MAGIC_NUMBER) {
            throw InvalidFormatError("Not a valid HPC compressed file");
        }

        if (header.majorVersion != VERSION_MAJOR) {
            throw InvalidFormatError("Unsupported version: " +
                                     std::to_string(header.majorVersion) + "." +
                                     std::to_string(header.minorVersion));
        }

        originalSize_ = header.originalSize;
        std::vector<uint8_t> decompressedData;
        decompressedData.reserve(originalSize_);

        for (uint32_t i = 0; i < header.blockCount; ++i) {
            BlockHeader bh;
            input.read(reinterpret_cast<char*>(&bh), sizeof(bh));

            std::vector<uint8_t> blockOutput;
            readAndDecompressBlock(input, bh, blockOutput);
            decompressedData.insert(decompressedData.end(),
                                    blockOutput.begin(), blockOutput.end());

            bool isCompressed = (bh.flags & BLOCK_FLAG_COMPRESSED) != 0;
            LOG_DEBUG("Block " + std::to_string(i + 1) + "/" +
                      std::to_string(header.blockCount) +
                      " flag=" + (isCompressed ? "compressed" : "stored") +
                      " size=" + std::to_string(bh.dataSize));
        }

        if (decompressedData.size() != originalSize_) {
            throw FileCorruptedError("Size mismatch: expected " +
                                     std::to_string(originalSize_) + ", got " +
                                     std::to_string(decompressedData.size()));
        }

        FileHandler output(outputPath, true);
        output.writeChunk(decompressedData, decompressedData.size());

        compressedSize_ = static_cast<size_t>(input.tellg());
        LOG_INFO("Decompression complete: " + std::to_string(originalSize_) + " bytes");

    } catch (const CompressionError& e) {
        LOG_ERROR(std::string("Decompression failed: ") + e.what());
        throw;
    }
}

void CompressionEngine::compressAndWriteBlock(std::ofstream& output,
                                               const std::vector<uint8_t>& data,
                                               size_t& totalCompressedSize) {
    // Phase 1: LZ77
    std::vector<LZ77Token> tokens = lz77_->compress(data);

    // Phase 2: Token → symbol stream
    std::vector<uint16_t> symbols;
    symbols.reserve(tokens.size() + tokens.size() / 2);

    for (const auto& token : tokens) {
        if (token.type == LZ77Token::Type::Literal) {
            symbols.push_back(static_cast<uint16_t>(token.literal));
        } else {
            symbols.push_back(256);
            symbols.push_back(token.offset);
            symbols.push_back(257);
            symbols.push_back(token.length);
        }
    }

    // Phase 3: Huffman
    std::vector<uint8_t> huffmanData = huffman_->compressSymbols(symbols);

    // Build compressed block payload: [symbolCount(4)] [huffmanData(N)]
    std::vector<uint8_t> compressedPayload;
    compressedPayload.resize(sizeof(uint32_t) + huffmanData.size());
    uint32_t symCount = static_cast<uint32_t>(symbols.size());
    std::memcpy(compressedPayload.data(), &symCount, sizeof(uint32_t));
    std::memcpy(compressedPayload.data() + sizeof(uint32_t),
                huffmanData.data(), huffmanData.size());

    // Smart fallback: compare compressed vs original
    if (compressedPayload.size() < data.size()) {
        BlockHeader bh = { BLOCK_FLAG_COMPRESSED,
                           static_cast<uint32_t>(compressedPayload.size()) };
        output.write(reinterpret_cast<const char*>(&bh), sizeof(bh));
        output.write(reinterpret_cast<const char*>(compressedPayload.data()),
                     compressedPayload.size());
        totalCompressedSize += sizeof(BlockHeader) + compressedPayload.size();
    } else {
        BlockHeader bh = { BLOCK_FLAG_STORED,
                           static_cast<uint32_t>(data.size()) };
        output.write(reinterpret_cast<const char*>(&bh), sizeof(bh));
        output.write(reinterpret_cast<const char*>(data.data()), data.size());
        totalCompressedSize += sizeof(BlockHeader) + data.size();
    }
}

void CompressionEngine::readAndDecompressBlock(std::ifstream& input,
                                                const BlockHeader& bh,
                                                std::vector<uint8_t>& output) {
    if (bh.flags & BLOCK_FLAG_COMPRESSED) {
        // Read compressed payload
        std::vector<uint8_t> compressedPayload(bh.dataSize);
        input.read(reinterpret_cast<char*>(compressedPayload.data()), bh.dataSize);

        // Extract symbol count
        uint32_t symbolCount;
        std::memcpy(&symbolCount, compressedPayload.data(), sizeof(uint32_t));

        // Extract Huffman data
        std::vector<uint8_t> huffmanData(
            compressedPayload.begin() + sizeof(uint32_t),
            compressedPayload.end());

        // Huffman decompress → symbols
        std::vector<uint8_t> rawSymbols = huffman_->decompressSymbols(
            huffmanData, symbolCount);

        // Symbol stream → LZ77 tokens
        std::vector<LZ77Token> tokens;
        tokens.reserve(symbolCount / 2);
        size_t i = 0;

        while (i < rawSymbols.size()) {
            uint16_t sym = rawSymbols[i++];

            if (sym == 256) {
                if (i + 2 > rawSymbols.size()) {
                    throw FileCorruptedError("Truncated match token in block");
                }
                uint16_t offset = rawSymbols[i++];
                uint16_t marker = rawSymbols[i++];
                uint16_t length = rawSymbols[i++];
                if (marker == 257) {
                    tokens.push_back(LZ77Token::matchToken(
                        static_cast<uint16_t>(offset),
                        static_cast<uint8_t>(length)));
                }
            } else if (sym < 256) {
                tokens.push_back(LZ77Token::literalToken(
                    static_cast<uint8_t>(sym)));
            }
        }

        // LZ77 decompress
        output = lz77_->decompress(tokens);
    } else {
        // Store mode: copy raw bytes directly
        output.resize(bh.dataSize);
        if (bh.dataSize > 0) {
            input.read(reinterpret_cast<char*>(output.data()), bh.dataSize);
        }
    }
}

double CompressionEngine::getCompressionRatio() const {
    if (originalSize_ == 0) return 0.0;
    return 100.0 * (1.0 - static_cast<double>(compressedSize_) / originalSize_);
}

} // namespace compressor
