# HPC — High Performance Compression Engine

**Hybrid LZ77 + Huffman compressor with per-block Smart Fallback (Format V2)**

[![Build](https://img.shields.io/badge/build-passing-brightgreen)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](#)
[![CMake](https://img.shields.io/badge/CMake-3.15+-064F8C.svg)](#)

---

## Architecture — Format V2 & Smart Fallback

The core differentiator of HPC is its **block-level Smart Fallback** mechanism. Instead of applying compression to the entire file as a single monolithic stream, HPC processes input data in independent **64 KB blocks**. Each block is individually evaluated:

```
Input File (e.g. 256 KB)
  ├── Block 0 (64 KB) → LZ77 + Huffman → compressed payload?  → STORED (raw copy)
  ├── Block 1 (64 KB) → LZ77 + Huffman → compressed payload?  → COMPRESSED
  ├── Block 2 (64 KB) → LZ77 + Huffman → compressed payload?  → COMPRESSED
  └── Block 3 (64 KB) → LZ77 + Huffman → compressed payload?  → STORED (raw copy)
```

**The problem it solves:** High-entropy files (PDFs, JPEGs, ZIPs, already-compressed binaries) do not compress well. Applying LZ77+Huffman to these files wastes CPU cycles and, worst case, *increases* the output size (incompressibility expansion). A naive whole-file compressor either always compresses (wasting time) or requires a slow heuristic pass.

**The Smart Fallback solution:** After compression, the engine compares the compressed payload size against the original block size. If the compressed representation is *larger*, the block is written as raw stored bytes instead. This is a **per-block, per-signal decision** — no heuristics, no guessing, zero waste.

### V2 Binary Format

```
┌──────────────────────────────────────────────────┐
│ CompressionHeader (15 bytes, #pragma pack(1))    │
│   magic        : uint32_t  (0x48445043 = "HDPC") │
│   majorVersion : uint8_t   (2)                   │
│   minorVersion : uint8_t   (0)                   │
│   originalSize : uint32_t                        │
│   blockSize    : uint32_t  (default: 65536)       │
│   blockCount   : uint32_t                        │
├──────────────────────────────────────────────────┤
│ Block 0..N                                       │
│ ┌────────────────────────────────────────────┐   │
│ │ BlockHeader (5 bytes, #pragma pack(1))     │   │
│ │   flags    : uint8_t  (0x00=STORED,        │   │
│ │                        0x01=COMPRESSED)    │   │
│ │   dataSize : uint32_t                      │   │
│ ├────────────────────────────────────────────┤   │
│ │ Payload (dataSize bytes)                   │   │
│ │   STORED:    raw block bytes               │   │
│ │   COMPRESSED: [symbolCount(4)] + Huffman   │   │
│ └────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────┘
```

All headers use `#pragma pack(1)` to eliminate compiler padding, ensuring a predictable 5-byte `BlockHeader` and 15-byte `CompressionHeader` across all platforms. This is critical for a portable binary format.

---

## Installation

### Prerequisites

- **Compiler**: GCC 9+, Clang 10+, or MSVC 2019+ (C++17 required)
- **Build system**: CMake 3.15+

### Build

```bash
git clone https://github.com/epadev/hpc-compressor.git
cd hpc-compressor
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

The binary is output to `build/bin/hpc`.

### Install (optional)

```bash
sudo cmake --install . --prefix /usr/local
```

---

## Usage

```bash
# Compress
./hpc -c input.txt -o input.txt.hpc

# Decompress
./hpc -d input.txt.hpc -o restored.txt

# Shorthand — compress (auto .hpc output)
./hpc input.bin

# Shorthand — decompress (auto-strip .hpc extension)
./hpc compressed.hpc

# Verbose mode
./hpc -c -v largefile.dat -o compressed.hpc

# Quiet mode (errors only)
./hpc -d -q archive.hpc -o archive.tar
```

### Options

| Flag | Description |
|------|-------------|
| `-c, --compress` | Compression mode |
| `-d, --decompress` | Decompression mode |
| `-i, --input FILE` | Input file |
| `-o, --output FILE` | Output file |
| `-v, --verbose` | Enable verbose logging |
| `-q, --quiet` | Suppress non-error output |
| `-V, --version` | Print version |
| `-h, --help` | Print usage |

---

## Benchmarks

> Fill in after running on your hardware.

| File Type | Original Size | Compressed Size | Ratio | Time | RAM |
|-----------|---------------|-----------------|-------|------|-----|
| Plain text (.txt) | | | | | |
| Source code (.cpp) | | | | | |
| JSON data (.json) | | | | | |
| Binary (.bin) | | | | | |
| PDF (.pdf) | | | | | |
| JPEG (.jpg) | | | | | |
| ZIP archive (.zip) | | | | | |

---

## License

[MIT](LICENSE)
