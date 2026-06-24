#include "core/Deflate.hpp"
#include <iostream>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <queue>
#include <map>

using namespace std;

namespace {
    const uint16_t MAX_VENTANA_BUSQUEDA = 4095;
    const uint8_t MAX_LONGITUD_COINCIDENCIA = 255;
    const int HASH_SIZE = 65536;
    const int MAX_CHAIN_DEPTH = 100;

    struct Tupla {
        uint16_t distancia;
        uint8_t longitud;
        char siguiente_caracter;
    };

    struct Nodo {
        char caracter;
        int frecuencia;
        int id;
        Nodo *izq, *der;

        Nodo(char c, int f, int& id_counter) : caracter(c), frecuencia(f), id(id_counter++), izq(nullptr), der(nullptr) {}
        Nodo(Nodo *i, Nodo *d, int& id_counter) : caracter('\0'), frecuencia(i->frecuencia + d->frecuencia), id(id_counter++), izq(i), der(d) {}
        ~Nodo() { delete izq; delete der; }
    };

    struct Comparar {
        bool operator()(Nodo *a, Nodo *b) {
            if (a->frecuencia == b->frecuencia) return a->id > b->id;
            return a->frecuencia > b->frecuencia;
        }
    };

    void generarCodigosDeflate(Nodo *raiz, string codigo, map<char, string> &diccionario) {
        if (!raiz) return;
        if (!raiz->izq && !raiz->der) diccionario[raiz->caracter] = codigo;
        generarCodigosDeflate(raiz->izq, codigo + "0", diccionario);
        generarCodigosDeflate(raiz->der, codigo + "1", diccionario);
    }
}

void Deflate::compress(std::istream& entrada, std::ostream& salida, std::function<void(size_t)> progress_callback) {
    const size_t CHUNK_SIZE = 4 * 1024 * 1024; 
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

        vector<char> buffer_lz77;
        int start_idx = historial.size();
        int end_idx = ventana.size();
        
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

            const char* dist_ptr = reinterpret_cast<const char*>(&mejor.distancia);
            buffer_lz77.insert(buffer_lz77.end(), dist_ptr, dist_ptr + sizeof(mejor.distancia));
            const char* long_ptr = reinterpret_cast<const char*>(&mejor.longitud);
            buffer_lz77.insert(buffer_lz77.end(), long_ptr, long_ptr + sizeof(mejor.longitud));
            buffer_lz77.push_back(mejor.siguiente_caracter);

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

        int id_counter = 0; 
        map<char, int> frecuencias;
        for (char c : buffer_lz77) frecuencias[c]++;

        priority_queue<Nodo *, vector<Nodo *>, Comparar> cola;
        for (auto par : frecuencias) cola.push(new Nodo(par.first, par.second, id_counter));

        while (cola.size() > 1) {
            Nodo *izq = cola.top(); cola.pop();
            Nodo *der = cola.top(); cola.pop();
            cola.push(new Nodo(izq, der, id_counter));
        }
        Nodo *raiz = cola.empty() ? nullptr : cola.top();

        map<char, string> diccionario;
        generarCodigosDeflate(raiz, "", diccionario);

        uint32_t totalCaracteresLZ77 = buffer_lz77.size();
        salida.write(reinterpret_cast<const char*>(&totalCaracteresLZ77), sizeof(totalCaracteresLZ77));
        uint32_t numCaracteresDistintos = frecuencias.size();
        salida.write(reinterpret_cast<const char*>(&numCaracteresDistintos), sizeof(numCaracteresDistintos));

        for (auto par : frecuencias) {
            salida.write(&par.first, sizeof(par.first));
            uint32_t freq = static_cast<uint32_t>(par.second);
            salida.write(reinterpret_cast<const char*>(&freq), sizeof(freq));
        }

        unsigned char buffer_bits = 0;
        int bitsEnBuffer = 0;
        for (char c : buffer_lz77) {
            string codigoBinario = diccionario[c];
            for (char bit : codigoBinario) {
                buffer_bits = buffer_bits << 1;
                if (bit == '1') buffer_bits = buffer_bits | 1;
                bitsEnBuffer++;
                if (bitsEnBuffer == 8) {
                    salida.write(reinterpret_cast<const char*>(&buffer_bits), sizeof(buffer_bits));
                    buffer_bits = 0;
                    bitsEnBuffer = 0;
                }
            }
        }
        if (bitsEnBuffer > 0) {
            buffer_bits = buffer_bits << (8 - bitsEnBuffer);
            salida.write(reinterpret_cast<const char*>(&buffer_bits), sizeof(buffer_bits));
        }

        delete raiz;

        size_t new_history_size = min(ventana.size(), HISTORY_SIZE);
        historial.assign(ventana.end() - new_history_size, ventana.end());

        total_processed += bytes_read;
        if (progress_callback) progress_callback(total_processed);
    }
}

void Deflate::decompress(std::istream& entrada, std::ostream& salida, std::function<void(size_t)> progress_callback) {
    const size_t HISTORY_SIZE = MAX_VENTANA_BUSQUEDA;
    vector<char> historial;
    size_t total_processed = 0;
    uint32_t chunk_bytes_to_process;

    while (entrada.read(reinterpret_cast<char*>(&chunk_bytes_to_process), sizeof(chunk_bytes_to_process))) {
        int id_counter = 0;
        uint32_t totalCaracteresLZ77;
        if (!entrada.read(reinterpret_cast<char*>(&totalCaracteresLZ77), sizeof(totalCaracteresLZ77))) break;

        uint32_t numCaracteresDistintos;
        entrada.read(reinterpret_cast<char*>(&numCaracteresDistintos), sizeof(numCaracteresDistintos));

        map<char, int> frecuencias;
        for (uint32_t i = 0; i < numCaracteresDistintos; i++) {
            char caracter; uint32_t freq;
            entrada.read(&caracter, sizeof(caracter));
            entrada.read(reinterpret_cast<char*>(&freq), sizeof(freq));
            frecuencias[caracter] = freq;
        }

        if (frecuencias.empty()) continue;

        priority_queue<Nodo *, vector<Nodo *>, Comparar> cola;
        for (auto par : frecuencias) cola.push(new Nodo(par.first, par.second, id_counter));

        while (cola.size() > 1) {
            Nodo *izq = cola.top(); cola.pop();
            Nodo *der = cola.top(); cola.pop();
            cola.push(new Nodo(izq, der, id_counter));
        }
        Nodo *raiz = cola.top();

        vector<char> buffer_lz77;
        buffer_lz77.reserve(totalCaracteresLZ77);
        
        Nodo *actual = raiz;
        uint32_t caracteresEscritos = 0;
        unsigned char buffer_bits;

        while (caracteresEscritos < totalCaracteresLZ77 && entrada.read(reinterpret_cast<char*>(&buffer_bits), sizeof(buffer_bits))) {
            for (int i = 7; i >= 0; i--) {
                int bit = (buffer_bits >> i) & 1;
                actual = (bit == 0) ? actual->izq : actual->der;

                if (!actual->izq && !actual->der) {
                    buffer_lz77.push_back(actual->caracter);
                    caracteresEscritos++;
                    actual = raiz;
                    if (caracteresEscritos == totalCaracteresLZ77) break;
                }
            }
        }
        delete raiz;

        vector<char> chunk_decodificado;
        int indexBuffer = 0;
        
        while (indexBuffer < buffer_lz77.size()) {
            Tupla tupla;
            tupla.distancia = *reinterpret_cast<uint16_t*>(&buffer_lz77[indexBuffer]);
            indexBuffer += sizeof(tupla.distancia);
            tupla.longitud = *reinterpret_cast<uint8_t*>(&buffer_lz77[indexBuffer]);
            indexBuffer += sizeof(tupla.longitud);
            tupla.siguiente_caracter = buffer_lz77[indexBuffer];
            indexBuffer += sizeof(tupla.siguiente_caracter);

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
                }
            }
            if (tupla.siguiente_caracter != '\0' || indexBuffer < buffer_lz77.size()) {
                chunk_decodificado.push_back(tupla.siguiente_caracter);
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