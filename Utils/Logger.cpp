#include "Utils/Logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace compressor {

Logger& Logger::instance() {
    static Logger instance_;
    return instance_;
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    minLevel_ = level;
}

void Logger::setQuiet(bool quiet) {
    std::lock_guard<std::mutex> lock(mutex_);
    quiet_ = quiet;
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::progress(const std::string& operation, size_t current, size_t total) {
    if (quiet_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    double percentage = (total > 0) ? (100.0 * current / total) : 0;
    
    std::cerr << "\r" << operation << ": " 
              << std::fixed << std::setprecision(1) << percentage << "%"
              << " (" << current << "/" << total << " bytes)"
              << std::string(20, ' ')
              << std::flush;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (quiet_ && level != LogLevel::ERROR) return;
    if (level < minLevel_) return;

    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::cerr << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
              << "[" << levelToString(level) << "] "
              << message << std::endl;
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace compressor
