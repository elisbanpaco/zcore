#include "Deflate.hpp"
#include <iostream>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <queue>
#include <map>

using namespace std;

namespace {

// ==========================================
// 1. CONFIGURACIONES Y ESTRUCTURAS LZ77
// ==========================================
const uint16_t MAX_VENTANA_BUSQUEDA = 4095;
const uint8_t MAX_LONGITUD_COINCIDENCIA = 255;

struct Tupla {
    uint16_t distancia;
    uint8_t longitud;
    char siguiente_caracter;
};

Tupla buscarMejorCoincidencia(const vector<char>& datos, int posicionActual) {
    Tupla mejor = {0, 0, datos[posicionActual]};
    int inicioBusqueda = max(0, posicionActual - MAX_VENTANA_BUSQUEDA);
    
    for (int j = inicioBusqueda; j < posicionActual; ++j) {
        int longitudActual = 0;
        while (longitudActual < MAX_LONGITUD_COINCIDENCIA && 
               posicionActual + longitudActual < datos.size() &&
               datos[j + longitudActual] == datos[posicionActual + longitudActual]) {
            longitudActual++;
        }
        if (longitudActual > mejor.longitud) {
            mejor.distancia = posicionActual - j;
            mejor.longitud = longitudActual;
            if (posicionActual + longitudActual < datos.size()) {
                mejor.siguiente_caracter = datos[posicionActual + longitudActual];
            } else {
                mejor.siguiente_caracter = '\0';
            }
        }
    }
    return mejor;
}

// ==========================================
// 2. ESTRUCTURAS HUFFMAN
// ==========================================
int id_global_deflate = 0;

struct Nodo {
    char caracter;
    int frecuencia;
    int id;
    Nodo *izq, *der;

    Nodo(char c, int f) : caracter(c), frecuencia(f), id(id_global_deflate++), izq(nullptr), der(nullptr) {}
    Nodo(Nodo *i, Nodo *d) : caracter('\0'), frecuencia(i->frecuencia + d->frecuencia), id(id_global_deflate++), izq(i), der(d) {}
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

} // namespace

// ==========================================
// 3. COMPRESIÓN DEFLATE (LZ77 -> Huffman)
// ==========================================
void Deflate::compress(std::istream& entrada, std::ostream& salida) {
    vector<char> datos((istreambuf_iterator<char>(entrada)), istreambuf_iterator<char>());
    if (datos.empty()) return;

    vector<char> buffer_lz77;
    int i = 0;
    while (i < datos.size()) {
        Tupla tupla = buscarMejorCoincidencia(datos, i);
        
        // Serializar la tupla al buffer intermedio
        const char* dist_ptr = reinterpret_cast<const char*>(&tupla.distancia);
        buffer_lz77.insert(buffer_lz77.end(), dist_ptr, dist_ptr + sizeof(tupla.distancia));
        
        const char* long_ptr = reinterpret_cast<const char*>(&tupla.longitud);
        buffer_lz77.insert(buffer_lz77.end(), long_ptr, long_ptr + sizeof(tupla.longitud));
        
        buffer_lz77.push_back(tupla.siguiente_caracter);
        
        i += tupla.longitud + 1;
    }

    id_global_deflate = 0; 
    map<char, int> frecuencias;
    for (char c : buffer_lz77) {
        frecuencias[c]++;
    }

    priority_queue<Nodo *, vector<Nodo *>, Comparar> cola;
    for (auto par : frecuencias) cola.push(new Nodo(par.first, par.second));

    while (cola.size() > 1) {
        Nodo *izq = cola.top(); cola.pop();
        Nodo *der = cola.top(); cola.pop();
        cola.push(new Nodo(izq, der));
    }
    Nodo *raiz = cola.top();

    map<char, string> diccionario;
    generarCodigosDeflate(raiz, "", diccionario);

    // Escribir cabecera Huffman
    int totalCaracteresLZ77 = buffer_lz77.size();
    salida.write((char *)&totalCaracteresLZ77, sizeof(totalCaracteresLZ77));
    size_t numCaracteresDistintos = frecuencias.size();
    salida.write((char *)&numCaracteresDistintos, sizeof(numCaracteresDistintos));

    for (auto par : frecuencias) {
        salida.write(&par.first, sizeof(par.first));
        salida.write((char *)&par.second, sizeof(par.second));
    }

    // Escribir datos empaquetados en bits
    unsigned char buffer_bits = 0;
    int bitsEnBuffer = 0;

    for (char c : buffer_lz77) {
        string codigoBinario = diccionario[c];
        for (char bit : codigoBinario) {
            buffer_bits = buffer_bits << 1;
            if (bit == '1') buffer_bits = buffer_bits | 1;
            bitsEnBuffer++;
            if (bitsEnBuffer == 8) {
                salida.write((char *)&buffer_bits, sizeof(buffer_bits));
                buffer_bits = 0;
                bitsEnBuffer = 0;
            }
        }
    }
    if (bitsEnBuffer > 0) {
        buffer_bits = buffer_bits << (8 - bitsEnBuffer);
        salida.write((char *)&buffer_bits, sizeof(buffer_bits));
    }
}

// ==========================================
// 4. DESCOMPRESIÓN DEFLATE (Huffman -> LZ77)
// ==========================================
void Deflate::decompress(std::istream& entrada, std::ostream& salida) {
    id_global_deflate = 0;
    int totalCaracteresLZ77;
    if (!entrada.read((char *)&totalCaracteresLZ77, sizeof(totalCaracteresLZ77))) return;

    size_t numCaracteresDistintos;
    entrada.read((char *)&numCaracteresDistintos, sizeof(numCaracteresDistintos));

    map<char, int> frecuencias;
    for (size_t i = 0; i < numCaracteresDistintos; i++) {
        char caracter; int freq;
        entrada.read(&caracter, sizeof(caracter));
        entrada.read((char *)&freq, sizeof(freq));
        frecuencias[caracter] = freq;
    }

    if (frecuencias.empty()) return;

    priority_queue<Nodo *, vector<Nodo *>, Comparar> cola;
    for (auto par : frecuencias) cola.push(new Nodo(par.first, par.second));

    while (cola.size() > 1) {
        Nodo *izq = cola.top(); cola.pop();
        Nodo *der = cola.top(); cola.pop();
        cola.push(new Nodo(izq, der));
    }
    Nodo *raiz = cola.top();

    vector<char> buffer_lz77;
    buffer_lz77.reserve(totalCaracteresLZ77);
    
    Nodo *actual = raiz;
    int caracteresEscritos = 0;
    unsigned char buffer_bits;

    while (entrada.read((char *)&buffer_bits, sizeof(buffer_bits)) && caracteresEscritos < totalCaracteresLZ77) {
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

    vector<char> textoDescomprimido;
    int indexBuffer = 0;
    
    // Reconstruir leyendo las tuplas desde el buffer desencriptado por Huffman (saltos de 4 bytes)
    while (indexBuffer < buffer_lz77.size()) {
        Tupla tupla;
        // Reconstruir distancia (2 bytes)
        tupla.distancia = *reinterpret_cast<uint16_t*>(&buffer_lz77[indexBuffer]);
        indexBuffer += sizeof(tupla.distancia);
        
        // Reconstruir longitud (1 byte)
        tupla.longitud = *reinterpret_cast<uint8_t*>(&buffer_lz77[indexBuffer]);
        indexBuffer += sizeof(tupla.longitud);
        
        // Siguiente caracter (1 byte)
        tupla.siguiente_caracter = buffer_lz77[indexBuffer];
        indexBuffer += sizeof(tupla.siguiente_caracter);

        if (tupla.distancia > 0) {
            int inicioCopia = textoDescomprimido.size() - tupla.distancia;
            for (int k = 0; k < tupla.longitud; ++k) {
                textoDescomprimido.push_back(textoDescomprimido[inicioCopia + k]);
            }
        }
        
        if (tupla.siguiente_caracter != '\0' || indexBuffer < buffer_lz77.size()) {
            textoDescomprimido.push_back(tupla.siguiente_caracter);
        }
    }

    salida.write(textoDescomprimido.data(), textoDescomprimido.size());
}