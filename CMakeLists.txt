cmake_minimum_required(VERSION 3.14)
project(ZCoreCompression)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

# 1. CLI11 (Parser Profesional de CLI)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.3.2
)
FetchContent_MakeAvailable(CLI11)

# 2. FTXUI (Terminal UI)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# 3. Catch2 (Testing)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# Include directories
include_directories(include)
include_directories(include/core)
include_directories(include/ui)

# Gather Core sources
set(CORE_SOURCES
    src/core/Huffman.cpp
    src/core/Lz77.cpp
    src/core/Deflate.cpp
    src/core/Gzip.cpp
)

# Main Executable
add_executable(compressor src/main.cpp ${CORE_SOURCES})
target_link_libraries(compressor PRIVATE ftxui::screen ftxui::dom ftxui::component CLI11::CLI11)

# Tests
add_executable(test_compression tests/test_compression.cpp ${CORE_SOURCES})
target_link_libraries(test_compression PRIVATE Catch2::Catch2WithMain)
