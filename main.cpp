#include <iostream>
#include <chrono>
#include <memory>

#include "CLI/ArgumentParser.hpp"
#include "Core/CompressionEngine.hpp"
#include "Utils/CompressionException.hpp"
#include "Utils/Logger.hpp"

int main(int argc, char* argv[]) {
    try {
        compressor::CLIOptions options = compressor::ArgumentParser::parse(argc, argv);
        
        if (options.mode == compressor::CLIOptions::Mode::Unknown) {
            compressor::ArgumentParser::printUsage(argv[0]);
            return 1;
        }
        
        compressor::Logger::instance().setQuiet(options.quiet);
        if (options.verbose) {
            compressor::Logger::instance().setLevel(compressor::LogLevel::DEBUG);
        }
        
        auto engine = std::make_unique<compressor::CompressionEngine>();
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        if (options.mode == compressor::CLIOptions::Mode::Compress) {
            LOG_INFO("=== HPC Compression Engine ===");
            LOG_INFO("Compressing: " + options.inputPath);
            LOG_INFO("Output: " + options.outputPath);
            
            engine->compress(options.inputPath, options.outputPath);
            
            LOG_INFO("Compression ratio: " + 
                std::to_string(engine->getCompressionRatio()) + "%");
            
        } else if (options.mode == compressor::CLIOptions::Mode::Decompress) {
            LOG_INFO("=== HPC Decompression Engine ===");
            LOG_INFO("Decompressing: " + options.inputPath);
            LOG_INFO("Output: " + options.outputPath);
            
            engine->decompress(options.inputPath, options.outputPath);
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        
        LOG_INFO("Time elapsed: " + std::to_string(duration.count()) + " ms");
        LOG_INFO("Original size: " + std::to_string(engine->getOriginalSize()) + " bytes");
        LOG_INFO("Compressed size: " + std::to_string(engine->getCompressedSize()) + " bytes");
        
        return 0;
        
    } catch (const compressor::CompressionError& e) {
        LOG_ERROR(std::string("Error: ") + e.what());
        return 1;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Unexpected error: ") + e.what());
        return 1;
    }
}
