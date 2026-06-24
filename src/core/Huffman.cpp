#include "core/Huffman.hpp"
#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <cstdint>

using namespace std;

namespace {
    struct Nodo {
        char caracter;
        int frecuencia;
        int id;
        Nodo *izq, *der;

        Nodo(char c, int f, int& id_counter) : caracter(c), frecuencia(f), id(id_counter++), izq(nullptr), der(nullptr) {}
        Nodo(Nodo *i, Nodo *d, int& id_counter) : caracter('\0'), frecuencia(i->frecuencia + d->frecuencia), id(id_counter++), izq(i), der(d) {}
        
        ~Nodo() {
            delete izq;
            delete der;
        }
    };

    struct Comparar {
        bool operator()(Nodo *a, Nodo *b) {
            if (a->frecuencia == b->frecuencia) {
                return a->id > b->id;
            }
            return a->frecuencia > b->frecuencia;
        }
    };

    void generarCodigos(Nodo *raiz, string codigo, map<char, string> &diccionario) {
        if (!raiz) return;
        if (!raiz->izq && !raiz->der) {
            diccionario[raiz->caracter] = codigo;
        }
        generarCodigos(raiz->izq, codigo + "0", diccionario);
        generarCodigos(raiz->der, codigo + "1", diccionario);
    }
}

void Huffman::compress(std::istream& entrada, std::ostream& salida, std::function<void(size_t)> progress_callback) {
    const size_t CHUNK_SIZE = 4 * 1024 * 1024; // 4MB
    vector<char> buffer(CHUNK_SIZE);
    size_t total_processed = 0;

    while (entrada) {
        entrada.read(buffer.data(), CHUNK_SIZE);
        size_t bytes_read = entrada.gcount();
        if (bytes_read == 0) break;

        map<char, int> frecuencias;
        for (size_t i = 0; i < bytes_read; ++i) {
            frecuencias[buffer[i]]++;
        }

        int id_counter = 0;
        priority_queue<Nodo*, vector<Nodo*>, Comparar> cola;
        for (auto par : frecuencias) {
            cola.push(new Nodo(par.first, par.second, id_counter));
        }

        while (cola.size() > 1) {
            Nodo *izq = cola.top(); cola.pop();
            Nodo *der = cola.top(); cola.pop();
            cola.push(new Nodo(izq, der, id_counter));
        }

        Nodo *raiz = cola.empty() ? nullptr : cola.top();

        map<char, string> diccionario;
        generarCodigos(raiz, "", diccionario);

        // Header del chunk
        uint32_t chunk_bytes = static_cast<uint32_t>(bytes_read);
        salida.write(reinterpret_cast<const char*>(&chunk_bytes), sizeof(chunk_bytes));
        
        uint32_t numCaracteresDistintos = static_cast<uint32_t>(frecuencias.size());
        salida.write(reinterpret_cast<const char*>(&numCaracteresDistintos), sizeof(numCaracteresDistintos));

        for (auto par : frecuencias) {
            salida.write(&par.first, sizeof(par.first));
            uint32_t freq = static_cast<uint32_t>(par.second);
            salida.write(reinterpret_cast<const char*>(&freq), sizeof(freq));
        }

        // Empaquetado
        unsigned char bits_buffer = 0;
        int bitsEnBuffer = 0;

        for (size_t i = 0; i < bytes_read; ++i) {
            string codigoBinario = diccionario[buffer[i]];
            for (char bit : codigoBinario) {
                bits_buffer = bits_buffer << 1;
                if (bit == '1') bits_buffer = bits_buffer | 1;
                bitsEnBuffer++;
                if (bitsEnBuffer == 8) {
                    salida.write(reinterpret_cast<const char*>(&bits_buffer), sizeof(bits_buffer));
                    bits_buffer = 0;
                    bitsEnBuffer = 0;
                }
            }
        }

        if (bitsEnBuffer > 0) {
            bits_buffer = bits_buffer << (8 - bitsEnBuffer);
            salida.write(reinterpret_cast<const char*>(&bits_buffer), sizeof(bits_buffer));
        }

        delete raiz; // Limpia la memoria del árbol

        total_processed += bytes_read;
        if (progress_callback) progress_callback(total_processed);
    }
}

void Huffman::decompress(std::istream& entrada, std::ostream& salida, std::function<void(size_t)> progress_callback) {
    uint32_t chunk_bytes;
    size_t total_processed = 0;

    while (entrada.read(reinterpret_cast<char*>(&chunk_bytes), sizeof(chunk_bytes))) {
        uint32_t numCaracteresDistintos;
        if (!entrada.read(reinterpret_cast<char*>(&numCaracteresDistintos), sizeof(numCaracteresDistintos))) break;

        map<char, int> frecuencias;
        for (uint32_t i = 0; i < numCaracteresDistintos; i++) {
            char caracter;
            uint32_t freq;
            entrada.read(&caracter, sizeof(caracter));
            entrada.read(reinterpret_cast<char*>(&freq), sizeof(freq));
            frecuencias[caracter] = freq;
        }

        if (frecuencias.empty()) continue;

        int id_counter = 0;
        priority_queue<Nodo*, vector<Nodo*>, Comparar> cola;
        for (auto par : frecuencias) {
            cola.push(new Nodo(par.first, par.second, id_counter));
        }

        while (cola.size() > 1) {
            Nodo *izq = cola.top(); cola.pop();
            Nodo *der = cola.top(); cola.pop();
            cola.push(new Nodo(izq, der, id_counter));
        }
        Nodo *raiz = cola.top();

        Nodo *actual = raiz;
        uint32_t caracteresEscritos = 0;
        unsigned char buffer;

        while (caracteresEscritos < chunk_bytes && entrada.read(reinterpret_cast<char*>(&buffer), sizeof(buffer))) {
            for (int i = 7; i >= 0; i--) {
                int bit = (buffer >> i) & 1;
                actual = (bit == 0) ? actual->izq : actual->der;

                if (!actual->izq && !actual->der) {
                    salida.write(&actual->caracter, sizeof(actual->caracter));
                    caracteresEscritos++;
                    actual = raiz;
                    if (caracteresEscritos == chunk_bytes) break;
                }
            }
        }

        delete raiz;

        total_processed += chunk_bytes;
        if (progress_callback) progress_callback(total_processed);
    }
}