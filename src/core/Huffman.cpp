#include "Huffman.hpp"
#include <iostream>
#include <vector>
#include <queue>
#include <map>

using namespace std;

namespace {

// Variable global para asegurar desempates deterministas en el árbol
int id_global = 0;

struct Nodo {
    char caracter;
    int frecuencia;
    int id; // ID único para desempates
    Nodo *izq, *der;

    Nodo(char c, int f) : caracter(c), frecuencia(f), id(id_global++), izq(nullptr), der(nullptr) {}
    Nodo(Nodo *i, Nodo *d) : caracter('\0'), frecuencia(i->frecuencia + d->frecuencia), id(id_global++), izq(i), der(d) {}
};

struct Comparar {
    bool operator()(Nodo *a, Nodo *b) {
        if (a->frecuencia == b->frecuencia) {
            return a->id > b->id; // ¡El desempate salva la vida!
        }
        return a->frecuencia > b->frecuencia;
    }
};

void generarCodigos(Nodo *raiz, string codigo, map<char, string> &diccionario) {
    if (!raiz) {
        return;
    }

    if (!raiz->izq && !raiz->der) {
        diccionario[raiz->caracter] = codigo;
    }

    generarCodigos(raiz->izq, codigo + "0", diccionario);
    generarCodigos(raiz->der, codigo + "1", diccionario);
}

} // namespace

void Huffman::compress(std::istream& entrada, std::ostream& salida) {
    id_global = 0; // Reiniciar ID
    
    // Cargar todo en memoria (para no tener que usar streamsize ni tellg/seekg que pueden fallar en streams puros)
    vector<char> datos((istreambuf_iterator<char>(entrada)), istreambuf_iterator<char>());
    
    map<char, int> frecuencias; // Ahora es ordenado
    int totalCaracteres = 0;

    for (char c : datos) {
        frecuencias[c]++;
        totalCaracteres++;
    }

    if (datos.empty()) return;

    priority_queue<Nodo *, vector<Nodo *>, Comparar> cola;
    for (auto par : frecuencias) {
        cola.push(new Nodo(par.first, par.second));
    }

    while (cola.size() > 1) { // Construir el árbol en base a las Comparaciones
        Nodo *izq = cola.top();
        cola.pop();
        Nodo *der = cola.top();
        cola.pop();
        cola.push(new Nodo(izq, der));
    }

    Nodo *raiz = cola.top();

    map<char, string> diccionario;
    generarCodigos(raiz, "", diccionario);

    // CABECERA
    salida.write((char *)&totalCaracteres, sizeof(totalCaracteres));
    size_t numCaracteresDistintos = frecuencias.size();
    salida.write((char *)&numCaracteresDistintos, sizeof(numCaracteresDistintos));

    for (auto par : frecuencias) {
        salida.write(&par.first, sizeof(par.first));
        salida.write((char *)&par.second, sizeof(par.second));
    }

    // EMPAQUETADO
    unsigned char buffer = 0;
    int bitsEnBuffer = 0;

    for (char c : datos) {
        string codigoBinario = diccionario[c];
        for (char bit : codigoBinario) {
            buffer = buffer << 1;

            if (bit == '1') {
                buffer = buffer | 1;
            }

            bitsEnBuffer++;

            if (bitsEnBuffer == 8) {
                salida.write((char *)&buffer, sizeof(buffer));
                buffer = 0;
                bitsEnBuffer = 0;
            }
        }
    }

    if (bitsEnBuffer > 0) {
        buffer = buffer << (8 - bitsEnBuffer);
        salida.write((char *)&buffer, sizeof(buffer));
    }
}

void Huffman::decompress(std::istream& entrada, std::ostream& salida) {
    id_global = 0; // Reiniciar ID para que asigne exactamente los mismos

    int totalCaracteresOriginales;
    if (!entrada.read((char *)&totalCaracteresOriginales, sizeof(totalCaracteresOriginales))) return;

    size_t numCaracteresDistintos;
    entrada.read((char *)&numCaracteresDistintos, sizeof(numCaracteresDistintos));

    map<char, int> frecuencias; // Debe ser map también para iterar en el mismo orden
    for (size_t i = 0; i < numCaracteresDistintos; i++) {
        char caracter;
        int freq;
        entrada.read(&caracter, sizeof(caracter));
        entrada.read((char *)&freq, sizeof(freq));
        frecuencias[caracter] = freq;
    }

    if (frecuencias.empty()) return;

    priority_queue<Nodo *, vector<Nodo *>, Comparar> cola;
    for (auto par : frecuencias) {
        cola.push(new Nodo(par.first, par.second));
    }

    while (cola.size() > 1) {
        Nodo *izq = cola.top();
        cola.pop();
        Nodo *der = cola.top();
        cola.pop();
        cola.push(new Nodo(izq, der));
    }
    Nodo *raiz = cola.top();

    Nodo *actual = raiz;
    int caracteresEscritos = 0;
    unsigned char buffer;

    while (entrada.read((char *)&buffer, sizeof(buffer)) && caracteresEscritos < totalCaracteresOriginales) {
        for (int i = 7; i >= 0; i--) {
            int bit = (buffer >> i) & 1;

            if (bit == 0) {
                actual = actual->izq;
            } else {
                actual = actual->der;
            }

            if (!actual->izq && !actual->der) {
                salida.write(&actual->caracter, sizeof(actual->caracter));
                caracteresEscritos++;
                actual = raiz;
                if (caracteresEscritos == totalCaracteresOriginales)
                    break;
            }
        }
    }
}