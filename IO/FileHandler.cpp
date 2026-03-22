#include "IO/FileHandler.hpp"
#include "Utils/Logger.hpp"
#include <algorithm>

namespace compressor {

FileHandler::FileHandler(const std::string& path, bool isWrite, size_t chunkSize)
    : fileSize_(0), bytesProcessed_(0), chunkSize_(chunkSize) {
    
    if (isWrite) {
        stream_.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    } else {
        stream_.open(path, std::ios::in | std::ios::binary);
        if (stream_.is_open()) {
            stream_.seekg(0, std::ios::end);
            fileSize_ = stream_.tellg();
            stream_.seekg(0, std::ios::beg);
        }
    }

    if (!stream_.is_open()) {
        throw FileOpenError(path, isWrite);
    }

    LOG_DEBUG("FileHandler opened: " + path + 
              (isWrite ? " (write)" : " (read, size: " + std::to_string(fileSize_) + ")"));
}

FileHandler::~FileHandler() {
    if (stream_.is_open()) {
        stream_.close();
    }
}

FileHandler::FileHandler(FileHandler&& other) noexcept
    : stream_(std::move(other.stream_)),
      fileSize_(other.fileSize_),
      bytesProcessed_(other.bytesProcessed_),
      chunkSize_(other.chunkSize_) {
    other.stream_.setstate(std::ios::badbit);
}

FileHandler& FileHandler::operator=(FileHandler&& other) noexcept {
    if (this != &other) {
        if (stream_.is_open()) {
            stream_.close();
        }
        stream_ = std::move(other.stream_);
        fileSize_ = other.fileSize_;
        bytesProcessed_ = other.bytesProcessed_;
        chunkSize_ = other.chunkSize_;
        other.stream_.setstate(std::ios::badbit);
    }
    return *this;
}

size_t FileHandler::readChunk(std::vector<uint8_t>& buffer) {
    checkStreamState(false);
    
    buffer.resize(chunkSize_);
    stream_.read(reinterpret_cast<char*>(buffer.data()), chunkSize_);
    size_t bytesRead = stream_.gcount();
    buffer.resize(bytesRead);
    bytesProcessed_ += bytesRead;
    
    return bytesRead;
}

void FileHandler::writeChunk(const std::vector<uint8_t>& buffer, size_t bytesToWrite) {
    checkStreamState(true);
    
    size_t writeSize = std::min(bytesToWrite, buffer.size());
    if (writeSize > 0) {
        stream_.write(reinterpret_cast<const char*>(buffer.data()), writeSize);
        bytesProcessed_ += writeSize;
        
        if (!stream_) {
            throw FileWriteError("write operation failed");
        }
    }
}

void FileHandler::checkStreamState(bool forWrite) {
    if (stream_.bad()) {
        if (forWrite) {
            throw FileWriteError("stream in bad state");
        } else {
            throw FileReadError("stream in bad state");
        }
    }
}

ChunkReader::ChunkReader(FileHandler& handler) : handler_(handler) {}

bool ChunkReader::readNext(std::vector<uint8_t>& buffer) {
    return handler_.readChunk(buffer) > 0;
}

ChunkWriter::ChunkWriter(FileHandler& handler) : handler_(handler) {}

void ChunkWriter::write(const std::vector<uint8_t>& data, size_t bytesToWrite) {
    handler_.writeChunk(data, bytesToWrite);
}

} // namespace compressor
