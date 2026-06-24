#pragma once
#include "Compressor.hpp"

class Lz77 : public Compressor {
public:
    void compress(std::istream& in, std::ostream& out, std::function<void(size_t)> progress_callback = nullptr) override;
    void decompress(std::istream& in, std::ostream& out, std::function<void(size_t)> progress_callback = nullptr) override;
};
