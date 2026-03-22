#include "IO/BitBuffer.hpp"
#include <algorithm>

namespace compressor {

BitBufferWriter::BitBufferWriter(std::ofstream& stream)
    : stream_(stream), buffer_(0), bitPosition_(0), totalBits_(0) {}

BitBufferWriter::~BitBufferWriter() {
    flush();
}

void BitBufferWriter::writeBits(uint32_t value, size_t numBits) {
    for (size_t i = 0; i < numBits; ++i) {
        writeBit((value >> i) & 1);
    }
}

void BitBufferWriter::writeBit(bool bit) {
    if (bit) {
        buffer_ |= (1 << bitPosition_);
    }
    ++bitPosition_;
    ++totalBits_;

    if (bitPosition_ == 8) {
        stream_.write(reinterpret_cast<char*>(&buffer_), 1);
        buffer_ = 0;
        bitPosition_ = 0;
    }
}

void BitBufferWriter::flush() {
    if (bitPosition_ > 0) {
        stream_.write(reinterpret_cast<char*>(&buffer_), 1);
        buffer_ = 0;
        bitPosition_ = 0;
    }
}

BitBufferReader::BitBufferReader(std::ifstream& stream)
    : stream_(stream), buffer_(0), bitPosition_(8) {}

uint32_t BitBufferReader::readBits(size_t numBits) {
    uint32_t result = 0;
    for (size_t i = 0; i < numBits; ++i) {
        result |= (static_cast<uint32_t>(readBit()) << i);
    }
    return result;
}

bool BitBufferReader::readBit() {
    if (bitPosition_ >= 8) {
        if (!stream_.read(reinterpret_cast<char*>(&buffer_), 1)) {
            buffer_ = 0;
        }
        bitPosition_ = 0;
    }
    bool bit = (buffer_ >> bitPosition_) & 1;
    ++bitPosition_;
    return bit;
}

void BitBufferReader::flushToByte() {
    if (bitPosition_ > 0 && bitPosition_ < 8) {
        if (!stream_.read(reinterpret_cast<char*>(&buffer_), 1)) {
            buffer_ = 0;
        }
        bitPosition_ = 8;
    }
}

} // namespace compressor
