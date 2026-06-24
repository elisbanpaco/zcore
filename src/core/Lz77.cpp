#include "core/Lz77.hpp"
#include <iostream>
#include <vector>
#include <cstdint>
#include <algorithm>

using namespace std;

namespace {
    const uint16_t MAX_VENTANA_BUSQUEDA = 4095;
    const uint8_t MAX_LONGITUD_COINCIDENCIA = 255;

    struct Tupla {
        uint16_t distancia;
        uint8_t longitud;
        char siguiente_caracter;
    };

    Tupla buscarMejorCoincidencia(const vector<char>& ventana, int posicionActual) {
        Tupla mejor = {0, 0, ventana[posicionActual]};
        int inicioBusqueda = max(0, posicionActual - MAX_VENTANA_BUSQUEDA);
        
        for (int j = inicioBusqueda; j < posicionActual; ++j) {
            int longitudActual = 0;
            while (longitudActual < MAX_LONGITUD_COINCIDENCIA && 
                   posicionActual + longitudActual < ventana.size() &&
                   ventana[j + longitudActual] == ventana[posicionActual + longitudActual]) {
                longitudActual++;
            }
            if (longitudActual > mejor.longitud) {
                mejor.distancia = posicionActual - j;
                mejor.longitud = longitudActual;
                if (posicionActual + longitudActual < ventana.size()) {
                    mejor.siguiente_caracter = ventana[posicionActual + longitudActual];
                } else {
                    mejor.siguiente_caracter = '\0';
                }
            }
        }
        return mejor;
    }
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
        int i = start_idx;

        while (i < end_idx) {
            Tupla tupla = buscarMejorCoincidencia(ventana, i);
            
            salida.write(reinterpret_cast<const char*>(&tupla.distancia), sizeof(tupla.distancia));
            salida.write(reinterpret_cast<const char*>(&tupla.longitud), sizeof(tupla.longitud));
            salida.write(&tupla.siguiente_caracter, sizeof(tupla.siguiente_caracter));
            
            i += tupla.longitud + 1;
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