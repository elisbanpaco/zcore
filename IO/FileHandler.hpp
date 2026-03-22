#ifndef IO_FILEHANDLER_HPP
#define IO_FILEHANDLER_HPP

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "Utils/CompressionException.hpp"

namespace compressor {

constexpr size_t DEFAULT_CHUNK_SIZE = 64 * 1024;

class FileHandler {
public:
    FileHandler(const std::string& path, bool isWrite, size_t chunkSize = DEFAULT_CHUNK_SIZE);
    ~FileHandler();

    FileHandler(const FileHandler&) = delete;
    FileHandler& operator=(const FileHandler&) = delete;

    FileHandler(FileHandler&&) noexcept;
    FileHandler& operator=(FileHandler&&) noexcept;

    size_t readChunk(std::vector<uint8_t>& buffer);
    void writeChunk(const std::vector<uint8_t>& buffer, size_t bytesToWrite);

    size_t getFileSize() const { return fileSize_; }
    size_t getBytesProcessed() const { return bytesProcessed_; }
    bool isOpen() const { return stream_.is_open(); }
    bool eof() const { return stream_.eof(); }

private:
    void checkStreamState(bool forWrite);

    std::fstream stream_;
    size_t fileSize_;
    size_t bytesProcessed_;
    size_t chunkSize_;
};

class ChunkReader {
public:
    explicit ChunkReader(FileHandler& handler);
    
    bool readNext(std::vector<uint8_t>& buffer);
    bool hasMore() const { return !handler_.eof(); }
    size_t totalRead() const { return handler_.getBytesProcessed(); }

private:
    FileHandler& handler_;
};

class ChunkWriter {
public:
    explicit ChunkWriter(FileHandler& handler);
    
    void write(const std::vector<uint8_t>& data, size_t bytesToWrite);
    size_t totalWritten() const { return handler_.getBytesProcessed(); }

private:
    FileHandler& handler_;
};

} // namespace compressor

#endif // IO_FILEHANDLER_HPP
