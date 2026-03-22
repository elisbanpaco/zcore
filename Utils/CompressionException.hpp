#ifndef UTILS_ERRORS_HPP
#define UTILS_ERRORS_HPP

#include <exception>
#include <string>
#include <system_error>

namespace compressor {

class CompressionError : public std::runtime_error {
public:
    enum class Code {
        FileOpenFailed,
        FileReadFailed,
        FileWriteFailed,
        FileCorrupted,
        InvalidFormat,
        OutOfMemory,
        CompressionFailed,
        DecompressionFailed,
        InvalidArgument
    };

    CompressionError(Code code, const std::string& message)
        : std::runtime_error(message), code_(code) {}

    Code code() const noexcept { return code_; }
    
    static std::string toString(Code code) {
        switch (code) {
            case Code::FileOpenFailed: return "FileOpenFailed";
            case Code::FileReadFailed: return "FileReadFailed";
            case Code::FileWriteFailed: return "FileWriteFailed";
            case Code::FileCorrupted: return "FileCorrupted";
            case Code::InvalidFormat: return "InvalidFormat";
            case Code::OutOfMemory: return "OutOfMemory";
            case Code::CompressionFailed: return "CompressionFailed";
            case Code::DecompressionFailed: return "DecompressionFailed";
            case Code::InvalidArgument: return "InvalidArgument";
            default: return "Unknown";
        }
    }

private:
    Code code_;
};

class FileOpenError : public CompressionError {
public:
    FileOpenError(const std::string& path, bool isWrite)
        : CompressionError(
            Code::FileOpenFailed,
            "Failed to " + std::string(isWrite ? "open file for writing: " : "open file for reading: ") + path) {}
};

class FileReadError : public CompressionError {
public:
    FileReadError(const std::string& path)
        : CompressionError(Code::FileReadFailed, "Failed to read from file: " + path) {}
};

class FileWriteError : public CompressionError {
public:
    FileWriteError(const std::string& path)
        : CompressionError(Code::FileWriteFailed, "Failed to write to file: " + path) {}
};

class FileCorruptedError : public CompressionError {
public:
    FileCorruptedError(const std::string& detail = "")
        : CompressionError(
            Code::FileCorrupted,
            "File is corrupted" + (detail.empty() ? "" : ": " + detail)) {}
};

class InvalidFormatError : public CompressionError {
public:
    InvalidFormatError(const std::string& detail = "")
        : CompressionError(
            Code::InvalidFormat,
            "Invalid file format" + (detail.empty() ? "" : ": " + detail)) {}
};

class OutOfMemoryError : public CompressionError {
public:
    OutOfMemoryError()
        : CompressionError(Code::OutOfMemory, "Out of memory") {}
};

class InvalidArgumentError : public CompressionError {
public:
    InvalidArgumentError(const std::string& detail)
        : CompressionError(Code::InvalidArgument, "Invalid argument: " + detail) {}
};

} // namespace compressor

#endif // UTILS_ERRORS_HPP
