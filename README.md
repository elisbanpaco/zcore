# HPC - High Performance Compression Engine

**Hybrid LZ77 + Huffman Compression for Binary Data**

A professional-grade, production-ready file compression engine implementing a hybrid algorithm combining LZ77 dictionary-based compression with Huffman entropy encoding, similar to the DEFLATE algorithm used in zlib/gzip.

## Features

- **Hybrid Compression**: Combines LZ77 (sliding window, hash chains) with Huffman coding
- **Binary Data Support**: Handles arbitrary binary data, not just text
- **Chunk-Based Processing**: Memory-efficient streaming for large files
- **Modern C++**: C++17 with RAII, smart pointers, and noexcept
- **Professional Error Handling**: Custom exception hierarchy with clear error messages
- **Cross-Platform**: CMake-based build system for Linux, Windows, and macOS
- **Optimized**: Release builds with -O3, -march=native optimizations

## Architecture

```
compresor-file/
├── Core/                      # Core compression algorithms
│   ├── LZ77Compressor.hpp/cpp  # LZ77 with hash chain matching
│   ├── HuffmanCompressor.hpp/cpp # Adaptive Huffman coding
│   └── CompressionEngine.hpp/cpp # Main coordinator
├── IO/                        # File I/O layer
│   ├── BitBuffer.hpp/cpp      # Bit-level read/write operations
│   └── FileHandler.hpp/cpp    # RAII file handling with chunking
├── CLI/                       # Command-line interface
│   └── ArgumentParser.hpp/cpp # Robust argument parsing
├── Utils/                     # Utilities
│   ├── Logger.hpp/cpp         # Thread-safe logging
│   └── CompressionException.hpp # Custom exceptions
├── CMakeLists.txt             # Build configuration
└── main.cpp                   # Entry point
```

### Module Details

- **Core/LZ77Compressor**: Implements LZ77 with 32KB sliding window and hash chain acceleration for fast match finding
- **Core/HuffmanCompressor**: Canonical Huffman coding with tree serialization for consistent compression ratios
- **IO/BitBuffer**: Efficient bit-level I/O for Huffman code output
- **IO/FileHandler**: RAII-based file handling with chunk-based streaming
- **Utils/Logger**: Thread-safe singleton logger with multiple log levels

## Installation

### Prerequisites

- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.15+
- Make or Ninja build system

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/hpc-compressor.git
cd hpc-compressor

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Install (optional)
cmake --install . --prefix /usr/local
```

### Platform-Specific Notes

**Linux/macOS:**
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
```

**Windows (Visual Studio):**
```powershell
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

## Usage

### Basic Commands

```bash
# Compress a file
./hpc -c input.bin -o output.hpc

# Decompress a file
./hpc -d input.hpc -o output.bin

# Short form (auto-detect mode from extension)
./hpc input.txt input.txt.hpc

# Decompress (auto-detect .hpc extension)
./hpc compressed.hpc
```

### Command-Line Options

| Option | Description |
|--------|-------------|
| `-c, --compress` | Set compression mode |
| `-d, --decompress` | Set decompression mode |
| `-i, --input FILE` | Input file path |
| `-o, --output FILE` | Output file path |
| `-v, --verbose` | Enable verbose logging |
| `-q, --quiet` | Suppress non-error output |
| `-V, --version` | Show version information |
| `-h, --help` | Show help message |

### Examples

```bash
# Compress with verbose output
./hpc -c -v largefile.dat -o compressed.hpc

# Decompress suppressing progress messages
./hpc -d -q compressed.hpc -o restored.dat

# Streaming compression (auto-output naming)
./hpc myfile.bin    # Creates myfile.bin.hpc
./hpc compressed.hpc  # Restores to myfile.bin
```

## File Format

HPC files use a custom binary format with the following structure:

```
┌─────────────────────────────────┐
│ Header (16 bytes)               │
│  - Magic: 0x48445043 ("HDPC")  │
│  - Version (major.minor)       │
│  - Original size               │
│  - Compressed size             │
│  - Token count                 │
│  - Symbol data size            │
├─────────────────────────────────┤
│ Chunk Data                      │
│  - Raw literal bytes           │
├─────────────────────────────────┤
│ Huffman-Encoded Symbol Stream   │
│  - Serialized Huffman tree     │
│  - Encoded LZ77 tokens         │
└─────────────────────────────────┘
```

## Benchmarks

> **Note**: Fill in your benchmark results here.

### Compression Ratio (Compressibility Test Set)

| File Type | Original | Compressed | Ratio |
|-----------|----------|------------|-------|
| Text (enwik8) | 100 MB | TBD | TBD% |
| Binary (random) | 100 MB | TBD | TBD% |
| Image (PNG) | 50 MB | TBD | TBD% |
| Archive (.zip) | 100 MB | TBD | TBD% |

### Speed Benchmarks

| Operation | Throughput | Hardware |
|-----------|------------|----------|
| Compression | TBD MB/s | TBD |
| Decompression | TBD MB/s | TBD |

### Comparison with Other Tools

| Tool | Ratio | Speed |
|------|-------|-------|
| HPC | TBD | TBD |
| gzip -6 | TBD | TBD |
| zstd -3 | TBD | TBD |
| lz4 | TBD | TBD |

## Technical Details

### LZ77 Algorithm

The LZ77 compressor uses:
- **Sliding Window**: 32KB for match references
- **Lookahead Buffer**: 258 bytes maximum match length
- **Hash Chains**: O(1) average case match finding
- **Minimum Match**: 3 bytes (2-byte matches discarded)

### Huffman Coding

- **Canonical Codes**: Ensures consistent output
- **Adaptive Tree Building**: Per-file optimal trees
- **Symbol Space**: 256 literal bytes + special tokens (256=match marker, 257=length)

### Memory Usage

- **Chunk Size**: 64KB default buffer
- **Peak Memory**: ~2MB for typical files
- **Streaming**: Only one chunk in memory at a time

## Error Handling

The engine provides specific exceptions for different failure modes:

```cpp
try {
    engine.compress("input.bin", "output.hpc");
} catch (const FileOpenError& e) {
    // Handle file access issues
} catch (const FileCorruptedError& e) {
    // Handle corrupted archives
} catch (const InvalidFormatError& e) {
    // Handle unsupported format versions
}
```

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- LZ77 algorithm based on the original Lempel-Ziv 1977 paper
- Huffman coding based on David Huffman's 1952 paper
- DEFLATE format inspiration from zlib/gzip
