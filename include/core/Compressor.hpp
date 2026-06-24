#pragma once
#include <iostream>
#include <functional>

class Compressor {
public:
    virtual ~Compressor() = default;
    virtual void compress(std::istream& in, std::ostream& out, std::function<void(size_t)> progress_callback = nullptr) = 0;
    virtual void decompress(std::istream& in, std::ostream& out, std::function<void(size_t)> progress_callback = nullptr) = 0;
};
