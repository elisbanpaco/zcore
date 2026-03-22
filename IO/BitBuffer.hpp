#ifndef IO_BITBUFFER_HPP
#define IO_BITBUFFER_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <fstream>
#include <memory>

namespace compressor {

class BitBufferWriter {
public:
    explicit BitBufferWriter(std::ofstream& stream);
    ~BitBufferWriter();

    void writeBits(uint32_t value, size_t numBits);
    void writeBit(bool bit);
    void flush();

    size_t totalBitsWritten() const { return totalBits_; }

private:
    std::ofstream& stream_;
    uint8_t buffer_;
    size_t bitPosition_;
    size_t totalBits_;
};

class BitBufferReader {
public:
    explicit BitBufferReader(std::ifstream& stream);
    ~BitBufferReader() = default;

    uint32_t readBits(size_t numBits);
    bool readBit();
    void flushToByte();
    bool eof() const { return stream_.eof() && bitPosition_ >= 8; }

private:
    std::ifstream& stream_;
    uint8_t buffer_;
    size_t bitPosition_;
};

} // namespace compressor

#endif // IO_BITBUFFER_HPP
