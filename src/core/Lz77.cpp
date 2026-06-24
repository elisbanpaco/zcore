#include "core/Lz77.hpp"
#include <iostream>
#include <vector>
#include <cstdint>
#include <algorithm>

using namespace std;

namespace {
    const uint16_t MAX_VENTANA_BUSQUEDA = 4095;
    const uint8_t MAX_LONGITUD_COINCIDENCIA = 255;
    const int HASH_SIZE = 65536; // Hash table size for O(1) lookups
    const int MAX_CHAIN_DEPTH = 100; // Limit hash collision search depth for speed

    struct Tupla {
        uint16_t distancia;
        uint8_t longitud;
        char siguiente_caracter;
    };
}

void Lz77::compress(std::istream& entrada, std::ostream& salida, std::function<void(size_t)> progress_callback) {
    const size_t CHUNK_SIZE = 4 * 1024 * 1024; // 4MB
    const size_t HISTORY_SIZE = MAX_VENTANA_BUSQUEDA;

    vector<char> historial;
    vector<char> buffer(CHUNK_SIZE);
    size_t total_processed = 0;

    while (entrada) {
        entrada.read(buffer.data(), CHUNK_SIZE);
        size_t bytes_read = entrada.gcount();
        if (bytes_read == 0) break;

        vector<char> ventana;
        ventana.reserve(historial.size() + bytes_read);
        ventana.insert(ventana.end(), historial.begin(), historial.end());
        ventana.insert(ventana.end(), buffer.begin(), buffer.begin() + bytes_read);

        uint32_t chunk_bytes_to_process = static_cast<uint32_t>(bytes_read);
        salida.write(reinterpret_cast<const char*>(&chunk_bytes_to_process), sizeof(chunk_bytes_to_process));

        int start_idx = historial.size();
        int end_idx = ventana.size();
        
        // Fast LZ77 using Hash Table
        vector<int> head(HASH_SIZE, -1);
        vector<int> prev_match(ventana.size(), -1);

        for (int i = 0; i < start_idx - 2; ++i) {
            uint16_t hash = (static_cast<uint8_t>(ventana[i]) << 8) | static_cast<uint8_t>(ventana[i+1]);
            prev_match[i] = head[hash];
            head[hash] = i;
        }

        int i = start_idx;
        while (i < end_idx) {
            Tupla mejor = {0, 0, ventana[i]};

            if (i < end_idx - 2) {
                uint16_t hash = (static_cast<uint8_t>(ventana[i]) << 8) | static_cast<uint8_t>(ventana[i+1]);
                int match_idx = head[hash];
                
                prev_match[i] = head[hash];
                head[hash] = i;

                int depth = 0;
                while (match_idx != -1 && depth < MAX_CHAIN_DEPTH) {
                    int dist = i - match_idx;
                    if (dist > MAX_VENTANA_BUSQUEDA) break; 
                    
                    int len = 0;
                    while (len < MAX_LONGITUD_COINCIDENCIA && i + len < end_idx && ventana[match_idx + len] == ventana[i + len]) {
                        len++;
                    }
                    
                    if (len > mejor.longitud) {
                        mejor.distancia = dist;
                        mejor.longitud = len;
                        mejor.siguiente_caracter = (i + len < end_idx) ? ventana[i + len] : '\0';
                        if (len == MAX_LONGITUD_COINCIDENCIA) break; 
                    }
                    
                    match_idx = prev_match[match_idx];
                    depth++;
                }
            }

            salida.write(reinterpret_cast<const char*>(&mejor.distancia), sizeof(mejor.distancia));
            salida.write(reinterpret_cast<const char*>(&mejor.longitud), sizeof(mejor.longitud));
            salida.write(&mejor.siguiente_caracter, sizeof(mejor.siguiente_caracter));
            
            // Advance and update hash table
            for (int k = 1; k <= mejor.longitud; ++k) {
                int pos = i + k;
                if (pos < end_idx - 2) {
                    uint16_t hash = (static_cast<uint8_t>(ventana[pos]) << 8) | static_cast<uint8_t>(ventana[pos+1]);
                    prev_match[pos] = head[hash];
                    head[hash] = pos;
                }
            }
            
            i += mejor.longitud + 1;
        }

        size_t new_history_size = min(ventana.size(), HISTORY_SIZE);
        historial.assign(ventana.end() - new_history_size, ventana.end());

        total_processed += bytes_read;
        if (progress_callback) progress_callback(total_processed);
    }
}

void Lz77::decompress(std::istream& entrada, std::ostream& salida, std::function<void(size_t)> progress_callback) {
    const size_t HISTORY_SIZE = MAX_VENTANA_BUSQUEDA;
    vector<char> historial;
    size_t total_processed = 0;

    uint32_t chunk_bytes_to_process;
    while (entrada.read(reinterpret_cast<char*>(&chunk_bytes_to_process), sizeof(chunk_bytes_to_process))) {
        vector<char> chunk_decodificado;
        chunk_decodificado.reserve(chunk_bytes_to_process);

        int bytes_escritos = 0;
        while (bytes_escritos < chunk_bytes_to_process) {
            Tupla tupla;
            if (!entrada.read(reinterpret_cast<char*>(&tupla.distancia), sizeof(tupla.distancia))) break;
            entrada.read(reinterpret_cast<char*>(&tupla.longitud), sizeof(tupla.longitud));
            entrada.read(&tupla.siguiente_caracter, sizeof(tupla.siguiente_caracter));

            if (tupla.distancia > 0) {
                int back_dist = tupla.distancia;
                for (int k = 0; k < tupla.longitud; ++k) {
                    char copiado;
                    if (back_dist > chunk_decodificado.size()) {
                        int hist_idx = historial.size() - (back_dist - chunk_decodificado.size());
                        copiado = historial[hist_idx];
                    } else {
                        int local_idx = chunk_decodificado.size() - back_dist;
                        copiado = chunk_decodificado[local_idx];
                    }
                    chunk_decodificado.push_back(copiado);
                    bytes_escritos++;
                }
            }
            
            if (bytes_escritos < chunk_bytes_to_process) {
                chunk_decodificado.push_back(tupla.siguiente_caracter);
                bytes_escritos++;
            }
        }

        salida.write(chunk_decodificado.data(), chunk_decodificado.size());

        vector<char> ventana;
        ventana.reserve(historial.size() + chunk_decodificado.size());
        ventana.insert(ventana.end(), historial.begin(), historial.end());
        ventana.insert(ventana.end(), chunk_decodificado.begin(), chunk_decodificado.end());

        size_t new_history_size = min(ventana.size(), HISTORY_SIZE);
        historial.assign(ventana.end() - new_history_size, ventana.end());

        total_processed += chunk_bytes_to_process;
        if (progress_callback) progress_callback(total_processed);
    }
}