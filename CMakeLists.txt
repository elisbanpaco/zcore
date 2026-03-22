cmake_minimum_required(VERSION 3.15)
project(HPC VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build tests" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(UTILS_SOURCES
    Utils/Logger.cpp
    Utils/CompressionException.hpp
)

set(IO_SOURCES
    IO/BitBuffer.cpp
    IO/BitBuffer.hpp
    IO/FileHandler.cpp
    IO/FileHandler.hpp
)

set(CORE_SOURCES
    Core/LZ77Compressor.cpp
    Core/LZ77Compressor.hpp
    Core/HuffmanCompressor.cpp
    Core/HuffmanCompressor.hpp
    Core/CompressionEngine.cpp
    Core/CompressionEngine.hpp
)

set(CLI_SOURCES
    CLI/ArgumentParser.cpp
    CLI/ArgumentParser.hpp
)

add_executable(hpc
    main.cpp
    ${UTILS_SOURCES}
    ${IO_SOURCES}
    ${CORE_SOURCES}
    ${CLI_SOURCES}
)

target_include_directories(hpc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/IO
    ${CMAKE_CURRENT_SOURCE_DIR}/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/CLI
)

target_compile_options(hpc PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native -mtune=native>
    $<$<CONFIG:Debug>:-g -O0 -Wall -Wextra>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g>
)

target_link_libraries(hpc PRIVATE)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(hpc PRIVATE NDEBUG)
else()
    target_compile_definitions(hpc PRIVATE DEBUG)
endif()

install(TARGETS hpc RUNTIME DESTINATION bin)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
