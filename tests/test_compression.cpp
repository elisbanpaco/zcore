#include <catch2/catch_test_macros.hpp>
#include <streambuf>
#include <istream>
#include <ostream>
#include <vector>
#include <fstream>
#include <iostream>
#include <string>
#include "core/Deflate.hpp"
#include "core/Huffman.hpp"
#include "core/Lz77.hpp"
#include "core/Gzip.hpp"
#include <sstream>

// Virtual Input Stream (Genera datos al vuelo en RAM, engaña al algoritmo)
class VirtualStreambuf : public std::streambuf {
    size_t total_size;
    size_t current_pos;
    std::vector<char> pattern;
public:
    VirtualStreambuf(size_t size) : total_size(size), current_pos(0) {
        pattern.resize(4096);
        for(size_t i = 0; i < pattern.size(); ++i) {
            pattern[i] = static_cast<char>(i % 256); // Patrón matemático predecible
        }
    }
protected:
    std::streamsize xsgetn(char* s, std::streamsize n) override {
        if (current_pos >= total_size) return 0;
        std::streamsize bytes_to_read = std::min(static_cast<std::streamsize>(total_size - current_pos), n);
        
        std::streamsize written = 0;
        while(written < bytes_to_read) {
            std::streamsize chunk = std::min(bytes_to_read - written, static_cast<std::streamsize>(pattern.size()));
            std::copy(pattern.begin(), pattern.begin() + chunk, s + written);
            written += chunk;
        }
        
        current_pos += bytes_to_read;
        return bytes_to_read;
    }
    int_type underflow() override {
        if (current_pos >= total_size) return traits_type::eof();
        char c = pattern[current_pos % pattern.size()];
        setg(&c, &c, &c + 1);
        current_pos++;
        return traits_type::to_int_type(c);
    }
};

class VirtualInputStream : public std::istream {
    VirtualStreambuf buf;
public:
    VirtualInputStream(size_t size) : buf(size), std::istream(&buf) {}
};

// Null Output Stream (Agujero negro: consume los bytes sin escribirlos al disco)
class NullStreambuf : public std::streambuf {
protected:
    std::streamsize xsputn(const char* s, std::streamsize n) override { return n; }
    int_type overflow(int_type c) override { return c; }
};

class NullOutputStream : public std::ostream {
    NullStreambuf buf;
public:
    NullOutputStream() : std::ostream(&buf) {}
};

// Utilidad para extraer el consumo de Memoria Física en Linux
size_t getResidentMemoryUsageKB() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while(std::getline(status, line)) {
        if(line.substr(0, 6) == "VmRSS:") {
            size_t colon = line.find(':');
            size_t kb = line.find("kB");
            if (colon != std::string::npos && kb != std::string::npos) {
                std::string num = line.substr(colon + 1, kb - colon - 1);
                return std::stoull(num);
            }
        }
    }
    return 0;
}

TEST_CASE("Fase 3: Prueba de Estrés (Virtualizacion 5GB)", "[stress]") {
    // Simular un archivo colosal de 5 Gigabytes exactos!
    size_t GIGABYTES = 5;
    size_t VIRTUAL_FILE_SIZE = GIGABYTES * 1024 * 1024 * 1024ULL; 
    
    WARN("--------------------------------------------------");
    WARN("Iniciando Motor Deflate con archivo virtual de " << GIGABYTES << " GB...");
    
    VirtualInputStream in_stream(VIRTUAL_FILE_SIZE);
    NullOutputStream out_stream;
    Deflate compressor;

    size_t mem_before = getResidentMemoryUsageKB();
    WARN("Memoria Inicial (Base): " << mem_before / 1024 << " MB");

    size_t last_gb = 0;
    
    // Invocamos el motor pasándole el Callback para trackear progreso
    compressor.compress(in_stream, out_stream, [&](size_t processed){
        size_t current_gb = processed / (1024 * 1024 * 1024ULL);
        if (current_gb > last_gb) {
            WARN(" -> " << current_gb << " GB comprimidos...");
            last_gb = current_gb;
        }
    });

    size_t mem_after = getResidentMemoryUsageKB();
    WARN("Memoria Final: " << mem_after / 1024 << " MB");
    
    size_t mem_used_kb = mem_after >= mem_before ? (mem_after - mem_before) : 0;
    
    WARN("Consumo de RAM extra por el Motor: " << mem_used_kb / 1024 << " MB");
    WARN("--------------------------------------------------");

    // CONDICIÓN MATEMÁTICA DE APROBACIÓN (O(1) Space Complexity):
    // Memoria teórica calculada:
    // - buffer de lectura (Chunk): 4 MB
    // - ventana (Historial + Chunk): ~4 MB
    // - prev_match (Tabla Hash int32): 4 MB * 4 bytes = 16 MB
    // TOTAL Teórico: ~24 MB.
    // Límite fijado en 30 MB (30720 KB). ¡El uso de RAM es 100% independiente del tamaño del archivo!
    REQUIRE(mem_used_kb < 30720); 
}

TEST_CASE("Fase de Integridad: Compresión y Descompresión sin pérdidas (Lossless)", "[correctness]") {
    std::string original_data = "En un lugar de la Mancha, de cuyo nombre no quiero acordarme, "
                                "no ha mucho tiempo que vivia un hidalgo de los de lanza en astillero, "
                                "adarga antigua, rocin flaco y galgo corredor. "
                                "Repitiendo caracteres repetidos repetitivos repetidamente...";
    
    // Inyectamos repetitividad para probar las tablas hash de LZ77 y los árboles de Huffman
    for(int i = 0; i < 50; i++) original_data += " ZCore Engine V1.0 Test C++17! ";

    WARN("Verificando integridad 100% Lossless para " << original_data.size() << " bytes...");

    SECTION("Algoritmo Huffman") {
        std::stringstream in_stream(original_data);
        std::stringstream compressed_stream;
        std::stringstream decompressed_stream;
        
        Huffman huff;
        huff.compress(in_stream, compressed_stream, nullptr);
        compressed_stream.seekg(0);
        huff.decompress(compressed_stream, decompressed_stream, nullptr);
        
        REQUIRE(decompressed_stream.str() == original_data);
        WARN(" -> Huffman superado.");
    }

    SECTION("Algoritmo LZ77") {
        std::stringstream in_stream(original_data);
        std::stringstream compressed_stream;
        std::stringstream decompressed_stream;
        
        Lz77 lz;
        lz.compress(in_stream, compressed_stream, nullptr);
        compressed_stream.seekg(0);
        lz.decompress(compressed_stream, decompressed_stream, nullptr);
        
        REQUIRE(decompressed_stream.str() == original_data);
        WARN(" -> LZ77 superado.");
    }

    SECTION("Algoritmo Deflate") {
        std::stringstream in_stream(original_data);
        std::stringstream compressed_stream;
        std::stringstream decompressed_stream;
        
        Deflate def;
        def.compress(in_stream, compressed_stream, nullptr);
        compressed_stream.seekg(0);
        def.decompress(compressed_stream, decompressed_stream, nullptr);
        
        REQUIRE(decompressed_stream.str() == original_data);
        WARN(" -> Deflate superado.");
    }

    SECTION("Algoritmo Gzip") {
        std::stringstream in_stream(original_data);
        std::stringstream compressed_stream;
        std::stringstream decompressed_stream;
        
        Gzip gz;
        gz.compress(in_stream, compressed_stream, nullptr);
        compressed_stream.seekg(0);
        gz.decompress(compressed_stream, decompressed_stream, nullptr);
        
        REQUIRE(decompressed_stream.str() == original_data);
        WARN(" -> Gzip superado.");
    }
}
