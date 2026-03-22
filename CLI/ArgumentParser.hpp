#ifndef CLI_ARGUMENTPARSER_HPP
#define CLI_ARGUMENTPARSER_HPP

#include <string>
#include <vector>
#include <map>

namespace compressor {

struct CLIOptions {
    enum class Mode { Compress, Decompress, Unknown } mode = Mode::Unknown;
    std::string inputPath;
    std::string outputPath;
    bool verbose = false;
    bool quiet = false;
    int compressionLevel = 6;
};

class ArgumentParser {
public:
    static CLIOptions parse(int argc, char* argv[]);
    static void printUsage(const char* programName);
    static void printVersion();

private:
    static bool isFlag(const std::string& arg);
    static bool isShortFlag(const std::string& arg);
};

} // namespace compressor

#endif // CLI_ARGUMENTPARSER_HPP
