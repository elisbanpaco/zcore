#ifndef UTILS_LOGGER_HPP
#define UTILS_LOGGER_HPP

#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace compressor {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel level);
    void setQuiet(bool quiet);

    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

    void progress(const std::string& operation, size_t current, size_t total);

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const std::string& message);
    std::string levelToString(LogLevel level);

    LogLevel minLevel_ = LogLevel::INFO;
    bool quiet_ = false;
    std::mutex mutex_;
};

#define LOG_DEBUG(msg) compressor::Logger::instance().debug(msg)
#define LOG_INFO(msg) compressor::Logger::instance().info(msg)
#define LOG_WARNING(msg) compressor::Logger::instance().warning(msg)
#define LOG_ERROR(msg) compressor::Logger::instance().error(msg)

} // namespace compressor

#endif // UTILS_LOGGER_HPP
