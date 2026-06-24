#include "Gzip.hpp"
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
int id_global_gzip = 0;

struct Nodo {
    char caracter;
    int frecuencia;
    int id;
    Nodo *izq, *der;

    Nodo(char c, int f) : caracter(c), frecuencia(f), id(id_global_gzip++), izq(nullptr), der(nullptr) {}
    Nodo(Nodo *i, Nodo *d) : caracter('\0'), frecuencia(i->frecuencia + d->frecuencia), id(id_global_gzip++), izq(i), der(d) {}
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

// ==========================================
// 3. GENERACIÓN DE CRC32 (Verificación de integridad)
// ==========================================
uint32_t calcularCRC32(const vector<char>& datos) {
    uint32_t crc = 0xFFFFFFFF;
    for (char byte : datos) {
        uint8_t b = static_cast<uint8_t>(byte);
        crc ^= b;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320; 
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

} // namespace

// ==========================================
// 4. COMPRESIÓN GZIP (Header + Deflate + Trailer)
// ==========================================
void Gzip::compress(std::istream& entrada, std::ostream& salida) {
    vector<char> datosOriginales((istreambuf_iterator<char>(entrada)), istreambuf_iterator<char>());
    if (datosOriginales.empty()) return;

    uint32_t crc32 = calcularCRC32(datosOriginales);
    streamsize tamañoOriginal = datosOriginales.size();

    // Estructura del Header GZip
    uint8_t id1 = 0x1F;        
    uint8_t id2 = 0x8B;        
    uint8_t cm = 0x08;         
    uint8_t flg = 0x00;        
    uint32_t mtime = 0x00000000; 
    uint8_t xfl = 0x04;        
    uint8_t os = 0x03;         

    salida.write(reinterpret_cast<char*>(&id1), 1);
    salida.write(reinterpret_cast<char*>(&id2), 1);
    salida.write(reinterpret_cast<char*>(&cm), 1);
    salida.write(reinterpret_cast<char*>(&flg), 1);
    salida.write(reinterpret_cast<char*>(&mtime), 4);
    salida.write(reinterpret_cast<char*>(&xfl), 1);
    salida.write(reinterpret_cast<char*>(&os), 1);

    vector<char> buffer_lz77;
    int i = 0;
    while (i < datosOriginales.size()) {
        Tupla tupla = buscarMejorCoincidencia(datosOriginales, i);
        
        const char* dist_ptr = reinterpret_cast<const char*>(&tupla.distancia);
        buffer_lz77.insert(buffer_lz77.end(), dist_ptr, dist_ptr + sizeof(tupla.distancia));
        
        const char* long_ptr = reinterpret_cast<const char*>(&tupla.longitud);
        buffer_lz77.insert(buffer_lz77.end(), long_ptr, long_ptr + sizeof(tupla.longitud));
        
        buffer_lz77.push_back(tupla.siguiente_caracter);
        i += tupla.longitud + 1;
    }

    id_global_gzip = 0; 
    map<char, int> frecuencias;
    for (char c : buffer_lz77) frecuencias[c]++;

    priority_queue<Nodo *, vector<Nodo *>, Comparar> cola;
    for (auto par : frecuencias) cola.push(new Nodo(par.first, par.second));

    while (cola.size() > 1) {
        Nodo *izq = cola.top(); cola.pop();
        Nodo *der = cola.top(); cola.pop();
        cola.push(new Nodo(izq, der));
    }
    Nodo *raiz = cola.top();

    map<char, string> diccionario;
    generarCodigosGzip(raiz, "", diccionario);

    int totalCaracteresLZ77 = buffer_lz77.size();
    salida.write((char *)&totalCaracteresLZ77, sizeof(totalCaracteresLZ77));
    size_t numCaracteresDistintos = frecuencias.size();
    salida.write((char *)&numCaracteresDistintos, sizeof(numCaracteresDistintos));

    for (auto par : frecuencias) {
        salida.write(&par.first, sizeof(par.first));
        salida.write((char *)&par.second, sizeof(par.second));
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

    salida.write(reinterpret_cast<char*>(&crc32), 4);
    uint32_t isize = static_cast<uint32_t>(tamañoOriginal); 
    salida.write(reinterpret_cast<char*>(&isize), 4);
}

// ==========================================
// 5. DESCOMPRESIÓN GZIP (Validación + Extracción)
// ==========================================
void Gzip::decompress(std::istream& entrada, std::ostream& salida) {
    uint8_t id1, id2, cm, flg;
    if (!entrada.read(reinterpret_cast<char*>(&id1), 1)) return;
    entrada.read(reinterpret_cast<char*>(&id2), 1);
    entrada.read(reinterpret_cast<char*>(&cm), 1);
    entrada.read(reinterpret_cast<char*>(&flg), 1);

    if (id1 != 0x1F || id2 != 0x8B || cm != 0x08) {
        throw std::runtime_error("El stream no es un contenedor válido de GZip o no usa Deflate.");
    }

    // Saltar el resto del header fijo (4 bytes MTIME + 1 byte XFL + 1 byte OS = 6 bytes)
    char skipHeader[6];
    entrada.read(skipHeader, 6);

    id_global_gzip = 0;
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

    while (caracteresEscritos < totalCaracteresLZ77 && entrada.read((char *)&buffer_bits, sizeof(buffer_bits))) {
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
    
    while (indexBuffer < buffer_lz77.size()) {
        Tupla tupla;
        tupla.distancia = *reinterpret_cast<uint16_t*>(&buffer_lz77[indexBuffer]);
        indexBuffer += sizeof(tupla.distancia);
        
        tupla.longitud = *reinterpret_cast<uint8_t*>(&buffer_lz77[indexBuffer]);
        indexBuffer += sizeof(tupla.longitud);
        
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

    uint32_t crcEsperado, tamañoEsperado;
    entrada.read(reinterpret_cast<char*>(&crcEsperado), 4);
    entrada.read(reinterpret_cast<char*>(&tamañoEsperado), 4);

    uint32_t crcCalculado = calcularCRC32(textoDescomprimido);

    if (crcCalculado != crcEsperado || textoDescomprimido.size() != tamañoEsperado) {
        std::cerr << ">> [ALERTA] Los datos reconstruidos difieren de las sumas de control originales.\n";
        // Aun asi lo guardamos o podriamos lanzar una excepcion
    }

    salida.write(textoDescomprimido.data(), textoDescomprimido.size());
}