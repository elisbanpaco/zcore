#include "CLI/ArgumentParser.hpp"
#include "Utils/CompressionException.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace compressor {

CLIOptions ArgumentParser::parse(int argc, char* argv[]) {
    CLIOptions options;
    
    if (argc < 2) {
        options.mode = CLIOptions::Mode::Unknown;
        return options;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-c" || arg == "--compress") {
            options.mode = CLIOptions::Mode::Compress;
        } else if (arg == "-d" || arg == "--decompress") {
            options.mode = CLIOptions::Mode::Decompress;
        } else if (arg == "-i" || arg == "--input") {
            if (i + 1 < argc) {
                options.inputPath = argv[++i];
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                options.outputPath = argv[++i];
            }
        } else if (arg == "-v" || arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
            options.quiet = true;
        } else if (arg == "-V" || arg == "--version") {
            printVersion();
            std::exit(0);
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg.substr(0, 2) == "--") {
            options.inputPath = arg.substr(2);
            if (options.mode == CLIOptions::Mode::Unknown) {
                options.mode = CLIOptions::Mode::Compress;
            }
        } else if (arg[0] != '-' && options.inputPath.empty()) {
            options.inputPath = arg;
            if (options.mode == CLIOptions::Mode::Unknown) {
                options.mode = CLIOptions::Mode::Compress;
            }
        } else if (arg[0] != '-' && !options.inputPath.empty() && options.outputPath.empty()) {
            options.outputPath = arg;
        }
    }

    if (options.inputPath.empty()) {
        throw InvalidArgumentError("Input file is required");
    }

    if (options.mode == CLIOptions::Mode::Unknown) {
        throw InvalidArgumentError("Mode (compress/decompress) must be specified");
    }

    if (options.outputPath.empty()) {
        if (options.mode == CLIOptions::Mode::Compress) {
            options.outputPath = options.inputPath + ".hpc";
        } else {
            if (options.inputPath.size() > 4 && 
                options.inputPath.substr(options.inputPath.size() - 4) == ".hpc") {
                options.outputPath = options.inputPath.substr(0, options.inputPath.size() - 4);
            } else {
                options.outputPath = options.inputPath + ".decompressed";
            }
        }
    }

    return options;
}

void ArgumentParser::printUsage(const char* programName) {
    std::cout << "HPC - High Performance Compression Engine\n"
              << "\n"
              << "Usage: " << programName << " [OPTIONS] [INPUT_FILE] [OUTPUT_FILE]\n"
              << "\n"
              << "Options:\n"
              << "  -c, --compress      Set compression mode (default if no mode specified)\n"
              << "  -d, --decompress    Set decompression mode\n"
              << "  -i, --input FILE    Input file path\n"
              << "  -o, --output FILE  Output file path\n"
              << "  -v, --verbose      Enable verbose output\n"
              << "  -q, --quiet        Suppress non-error output\n"
              << "  -V, --version      Show version information\n"
              << "  -h, --help         Show this help message\n"
              << "\n"
              << "Examples:\n"
              << "  " << programName << " -c file.txt file.txt.hpc\n"
              << "  " << programName << " -d -i file.txt.hpc -o file_restored.txt\n"
              << "  " << programName << " file.bin\n";
}

void ArgumentParser::printVersion() {
    std::cout << "HPC Compression Engine v1.0\n"
              << "LZ77 + Huffman hybrid compression\n"
              << "Built with C++17\n";
}

} // namespace compressor
