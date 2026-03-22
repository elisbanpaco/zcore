#include "Core/CompressionEngine.hpp"
#include "IO/BitBuffer.hpp"
#include "IO/FileHandler.hpp"
#include <algorithm>
#include <cstring>

namespace compressor {

CompressionEngine::CompressionEngine()
    : lz77_(std::make_unique<LZ77Compressor>()),
      huffman_(std::make_unique<HuffmanCompressor>()),
      originalSize_(0),
      compressedSize_(0) {}

void CompressionEngine::compress(const std::string& inputPath, const std::string& outputPath) {
    LOG_INFO("Starting compression: " + inputPath + " -> " + outputPath);
    
    try {
        FileHandler input(inputPath, false);
        originalSize_ = input.getFileSize();
        
        std::vector<uint8_t> allData;
        std::vector<uint8_t> chunk;
        
        while (input.readChunk(chunk) > 0) {
            allData.insert(allData.end(), chunk.begin(), chunk.end());
            LOG_DEBUG("Read chunk, total: " + std::to_string(allData.size()) + " bytes");
        }
        
        LOG_INFO("File loaded, running LZ77 compression...");
        std::vector<LZ77Token> lz77Tokens = lz77_->compress(allData);
        
        LOG_INFO("Encoding tokens with Huffman...");
        std::vector<uint8_t> literalData;
        std::vector<uint16_t> symbols;
        
        for (const auto& token : lz77Tokens) {
            if (token.type == LZ77Token::Type::Literal) {
                symbols.push_back(token.literal);
            } else {
                symbols.push_back(256);
                symbols.push_back(token.offset);
                symbols.push_back(257);
                symbols.push_back(token.length);
            }
        }
        
        writeCompressedFile(outputPath, lz77Tokens, literalData, originalSize_);
        
        LOG_INFO("Compression complete!");
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
        
        std::vector<LZ77Token> tokens = readCompressedTokens(input, header);
        
        LOG_INFO("Reconstructing data from LZ77 tokens...");
        std::vector<uint8_t> decompressed = lz77_->decompress(tokens);
        
        if (decompressed.size() != header.originalSize) {
            throw FileCorruptedError("Size mismatch after decompression");
        }
        
        FileHandler output(outputPath, true);
        output.writeChunk(decompressed, decompressed.size());
        
        originalSize_ = header.originalSize;
        compressedSize_ = header.compressedSize;
        
        LOG_INFO("Decompression complete!");
        
    } catch (const CompressionError& e) {
        LOG_ERROR(std::string("Decompression failed: ") + e.what());
        throw;
    }
}

void CompressionEngine::writeCompressedFile(
    const std::string& path,
    const std::vector<LZ77Token>& lz77Tokens,
    const std::vector<uint8_t>& literalData,
    size_t originalSize) {
    
    std::ofstream output(path, std::ios::binary);
    if (!output.is_open()) {
        throw FileOpenError(path, true);
    }
    
    std::vector<uint8_t> chunkData;
    std::vector<uint16_t> symbols;
    
    for (const auto& token : lz77Tokens) {
        if (token.type == LZ77Token::Type::Literal) {
            chunkData.push_back(token.literal);
            symbols.push_back(token.literal);
        } else {
            symbols.push_back(256);
            symbols.push_back(token.offset);
            symbols.push_back(257);
            symbols.push_back(token.length);
        }
    }
    
    std::vector<uint8_t> huffmanData = huffman_->compressSymbols(symbols);
    
    size_t headerStart = output.tellp();
    CompressionHeader header = {
        MAGIC_NUMBER,
        VERSION_MAJOR,
        VERSION_MINOR,
        static_cast<uint32_t>(originalSize),
        0,
        static_cast<uint32_t>(lz77Tokens.size()),
        static_cast<uint32_t>(huffmanData.size())
    };
    
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    uint32_t chunkSize = static_cast<uint32_t>(chunkData.size());
    output.write(reinterpret_cast<const char*>(&chunkSize), sizeof(chunkSize));
    if (!chunkData.empty()) {
        output.write(reinterpret_cast<const char*>(chunkData.data()), chunkData.size());
    }
    
    output.write(reinterpret_cast<const char*>(huffmanData.data()), huffmanData.size());
    
    size_t fileEnd = output.tellp();
    compressedSize_ = fileEnd - headerStart;
    
    output.seekp(headerStart + offsetof(CompressionHeader, compressedSize));
    uint32_t compSize = static_cast<uint32_t>(compressedSize_);
    output.write(reinterpret_cast<const char*>(&compSize), sizeof(compSize));
    
    output.close();
}

std::vector<LZ77Token> CompressionEngine::readCompressedTokens(
    std::ifstream& stream,
    const CompressionHeader& header) {
    
    uint32_t chunkSize;
    stream.read(reinterpret_cast<char*>(&chunkSize), sizeof(chunkSize));
    
    std::vector<uint8_t> chunkData(chunkSize);
    if (chunkSize > 0) {
        stream.read(reinterpret_cast<char*>(chunkData.data()), chunkSize);
    }
    
    std::vector<uint8_t> huffmanData(header.symbolDataSize);
    stream.read(reinterpret_cast<char*>(huffmanData.data()), header.symbolDataSize);
    
    std::vector<uint8_t> symbolVec = huffman_->decompressSymbols(huffmanData, header.lz77TokenCount * 4);
    
    std::vector<LZ77Token> tokens;
    size_t i = 0;
    
    while (i < symbolVec.size()) {
        uint16_t sym = symbolVec[i++];
        
        if (sym == 256) {
            uint16_t offset = symbolVec[i++];
            uint16_t marker = symbolVec[i++];
            uint16_t length = symbolVec[i++];
            if (marker == 257) {
                tokens.push_back(LZ77Token::matchToken(static_cast<uint16_t>(offset), 
                                                       static_cast<uint8_t>(length)));
            }
        } else if (sym < 256) {
            tokens.push_back(LZ77Token::literalToken(static_cast<uint8_t>(sym)));
        }
    }
    
    return tokens;
}

double CompressionEngine::getCompressionRatio() const {
    if (originalSize_ == 0) return 0.0;
    return 100.0 * (1.0 - static_cast<double>(compressedSize_) / originalSize_);
}

} // namespace compressor
