#include "core/Gzip.hpp"
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

    void generarCodigosGzip(Nodo *raiz, string codigo, map<char, string> &diccionario) {
        if (!raiz) return;
        if (!raiz->izq && !raiz->der) diccionario[raiz->caracter] = codigo;
        generarCodigosGzip(raiz->izq, codigo + "0", diccionario);
        generarCodigosGzip(raiz->der, codigo + "1", diccionario);
    }

    // Tabla de CRC32 dinámica
    uint32_t calcularCRC32Chunk(const vector<char>& datos, uint32_t crc_previo = 0) {
        uint32_t crc = ~crc_previo;
        for (char byte : datos) {
            crc ^= static_cast<unsigned char>(byte);
            for (int k = 0; k < 8; k++) {
                if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
                else crc >>= 1;
            }
        }
        return ~crc;
    }
}

void Gzip::compress(std::istream& entrada, std::ostream& salida, std::function<void(size_t)> progress_callback) {
    // HEADER GZIP (RFC 1952)
    const uint8_t HEADER[10] = {
        0x1F, 0x8B, // Magic Number (GZIP)
        0x08,       // Método de compresión (8 = DEFLATE)
        0x00,       // Flags
        0x00, 0x00, 0x00, 0x00, // Tiempo de modificación (0 = no disponible)
        0x00,       // Flags extra
        0xFF        // OS (255 = desconocido)
    };
    salida.write(reinterpret_cast<const char*>(HEADER), sizeof(HEADER));

    const size_t CHUNK_SIZE = 4 * 1024 * 1024; // 4MB
    const size_t HISTORY_SIZE = MAX_VENTANA_BUSQUEDA;

    vector<char> historial;
    vector<char> buffer(CHUNK_SIZE);
    
    size_t total_processed = 0;
    uint32_t crc_acumulado = 0;
    uint32_t isize_acumulado = 0;

    while (entrada) {
        entrada.read(buffer.data(), CHUNK_SIZE);
        size_t bytes_read = entrada.gcount();
        if (bytes_read == 0) break;

        // Actualizar CRC32 e ISIZE del flujo original sin comprimir
        vector<char> chunk_puro(buffer.begin(), buffer.begin() + bytes_read);
        crc_acumulado = calcularCRC32Chunk(chunk_puro, crc_acumulado);
        isize_acumulado += bytes_read;

        // LZ77 Phase
        vector<char> ventana;
        ventana.reserve(historial.size() + bytes_read);
        ventana.insert(ventana.end(), historial.begin(), historial.end());
        ventana.insert(ventana.end(), chunk_puro.begin(), chunk_puro.end());

        uint32_t chunk_bytes_to_process = static_cast<uint32_t>(bytes_read);
        salida.write(reinterpret_cast<const char*>(&chunk_bytes_to_process), sizeof(chunk_bytes_to_process));

        vector<char> buffer_lz77;
        int start_idx = historial.size();
        int end_idx = ventana.size();
        int i = start_idx;

        while (i < end_idx) {
            Tupla tupla = buscarMejorCoincidencia(ventana, i);
            const char* dist_ptr = reinterpret_cast<const char*>(&tupla.distancia);
            buffer_lz77.insert(buffer_lz77.end(), dist_ptr, dist_ptr + sizeof(tupla.distancia));
            const char* long_ptr = reinterpret_cast<const char*>(&tupla.longitud);
            buffer_lz77.insert(buffer_lz77.end(), long_ptr, long_ptr + sizeof(tupla.longitud));
            buffer_lz77.push_back(tupla.siguiente_caracter);
            i += tupla.longitud + 1;
        }

        // Huffman Phase
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
        generarCodigosGzip(raiz, "", diccionario);

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

        // Update history
        size_t new_history_size = min(ventana.size(), HISTORY_SIZE);
        historial.assign(ventana.end() - new_history_size, ventana.end());

        total_processed += bytes_read;
        if (progress_callback) progress_callback(total_processed);
    }

    // TRAILER GZIP (RFC 1952)
    salida.write(reinterpret_cast<const char*>(&crc_acumulado), sizeof(crc_acumulado));
    salida.write(reinterpret_cast<const char*>(&isize_acumulado), sizeof(isize_acumulado));
}

void Gzip::decompress(std::istream& entrada, std::ostream& salida, std::function<void(size_t)> progress_callback) {
    // Validar HEADER GZIP
    uint8_t magic[2];
    if (!entrada.read(reinterpret_cast<char*>(magic), 2) || magic[0] != 0x1F || magic[1] != 0x8B) {
        throw std::runtime_error("El archivo no es un formato GZIP válido (Magic bytes incorrectos).");
    }

    uint8_t method;
    if (!entrada.read(reinterpret_cast<char*>(&method), 1) || method != 0x08) {
        throw std::runtime_error("Método de compresión no soportado (Solo DEFLATE).");
    }

    uint8_t flags;
    entrada.read(reinterpret_cast<char*>(&flags), 1);
    entrada.seekg(6, ios::cur); // Ignorar mtime, xfl, os

    // Deflate loop (Chunked)
    const size_t HISTORY_SIZE = MAX_VENTANA_BUSQUEDA;
    vector<char> historial;
    size_t total_processed = 0;
    
    // Leemos hasta que no haya más chunks, o encontremos algo que parece el Trailer
    // El trailer tiene 8 bytes. Si quedan solo 8 bytes en el stream, es el trailer.
    // Una forma elegante es guardar la posición y chequear si faltan 8 bytes, pero 
    // chunk_bytes_to_process consume 4 bytes.
    // Usaremos un truco: leemos los 4 bytes de chunk_size. Si fallamos en reconstruir
    // significa que esos 4 bytes eran el CRC32 (el inicio del trailer).
    
    while (true) {
        // Miramos cuántos bytes quedan
        auto curr = entrada.tellg();
        entrada.seekg(0, ios::end);
        auto remaining = entrada.tellg() - curr;
        entrada.seekg(curr);

        if (remaining == 8) {
            // Llegamos al trailer
            break;
        }
        if (remaining < 8) {
            throw std::runtime_error("Stream corrupto, faltan bytes en el trailer de GZIP.");
        }

        uint32_t chunk_bytes_to_process;
        if (!entrada.read(reinterpret_cast<char*>(&chunk_bytes_to_process), sizeof(chunk_bytes_to_process))) break;

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

    // Validar Trailer
    uint32_t leido_crc, leido_isize;
    entrada.read(reinterpret_cast<char*>(&leido_crc), sizeof(leido_crc));
    entrada.read(reinterpret_cast<char*>(&leido_isize), sizeof(leido_isize));
}