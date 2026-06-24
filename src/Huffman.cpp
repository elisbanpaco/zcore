#include <iostream>
#include <fstream> // Para manejar archivos
#include <vector>
#include <queue>
#include <map> // Cambiado a map ordenado

using namespace std;

// Variable global para asegurar desempates deterministas en el árbol
int id_global = 0;

// --- 1. ESTRUCTURA DEL ÁRBOL ---
struct Nodo
{
    char caracter;
    int frecuencia;
    int id; // ID único para desempates
    Nodo *izq, *der;

    Nodo(char c, int f) : caracter(c), frecuencia(f), id(id_global++), izq(nullptr), der(nullptr) {}
    Nodo(Nodo *i, Nodo *d) : caracter('\0'), frecuencia(i->frecuencia + d->frecuencia), id(id_global++), izq(i), der(d) {}
};

struct Comparar
{
    bool operator()(Nodo *a, Nodo *b)
    {
        if (a->frecuencia == b->frecuencia)
        {
            return a->id > b->id; // ¡El desempate salva la vida!
        }
        return a->frecuencia > b->frecuencia;
    }
};

void generarCodigos(Nodo *raiz, string codigo, map<char, string> &diccionario)
{
    if (!raiz)
    {
        return;
    }

    if (!raiz->izq && !raiz->der)
    {
        diccionario[raiz->caracter] = codigo;
    }

    generarCodigos(raiz->izq, codigo + "0", diccionario);
    generarCodigos(raiz->der, codigo + "1", diccionario);
}

// --- 2. COMPRESIÓN REAL ---
void comprimir(const string &archivoEntrada, const string &archivoSalida)
{
    id_global = 0; // Reiniciar ID
    ifstream entrada(archivoEntrada, ios::binary);
    if (!entrada)
    {
        cout << "Error leyendo entrada.\n";
        return;
    }

    map<char, int> frecuencias; // Ahora es ordenado
    char c;

    int totalCaracteres = 0;

    while (entrada.get(c))
    {
        frecuencias[c]++;
        totalCaracteres++;
    }

    entrada.clear();
    entrada.seekg(0);

    priority_queue<Nodo *, vector<Nodo *>, Comparar> cola;
    for (auto par : frecuencias)
    {
        cola.push(new Nodo(par.first, par.second));
    }

    while (cola.size() > 1) // Construir el árbol en base a las Comparaciones
    {
        Nodo *izq = cola.top();
        cola.pop();
        Nodo *der = cola.top();
        cola.pop();
        cola.push(new Nodo(izq, der));
    }

    Nodo *raiz = cola.top();

    map<char, string> diccionario;
    generarCodigos(raiz, "", diccionario);

    ofstream salida(archivoSalida, ios::binary);

    // CABECERA
    salida.write((char *)&totalCaracteres, sizeof(totalCaracteres));
    size_t numCaracteresDistintos = frecuencias.size();
    salida.write((char *)&numCaracteresDistintos, sizeof(numCaracteresDistintos));

    for (auto par : frecuencias)
    {
        salida.write(&par.first, sizeof(par.first));
        salida.write((char *)&par.second, sizeof(par.second));
    }

    // EMPAQUETADO
    unsigned char buffer = 0;

    int bitsEnBuffer = 0;

    while (entrada.get(c))
    {
        string codigoBinario = diccionario[c];
        for (char bit : codigoBinario)
        {
            buffer = buffer << 1;

            if (bit == '1')
            {
                buffer = buffer | 1;
            }

            bitsEnBuffer++;

            if (bitsEnBuffer == 8)
            {
                salida.write((char *)&buffer, sizeof(buffer));
                buffer = 0;
                bitsEnBuffer = 0;
            }
        }
    }

    if (bitsEnBuffer > 0)
    {
        buffer = buffer << (8 - bitsEnBuffer);
        salida.write((char *)&buffer, sizeof(buffer));
    }
}

// --- 3. DESCOMPRESIÓN REAL ---
void descomprimir(const string &archivoEntrada, const string &archivoSalida)
{
    id_global = 0; // Reiniciar ID para que asigne exactamente los mismos
    ifstream entrada(archivoEntrada, ios::binary);

    if (!entrada)
    {
        cout << "Error leyendo archivo comprimido.\n";
        return;
    }

    int totalCaracteresOriginales;
    entrada.read((char *)&totalCaracteresOriginales, sizeof(totalCaracteresOriginales));

    size_t numCaracteresDistintos;
    entrada.read((char *)&numCaracteresDistintos, sizeof(numCaracteresDistintos));

    map<char, int> frecuencias; // Debe ser map también para iterar en el mismo orden
    for (size_t i = 0; i < numCaracteresDistintos; i++)
    {
        char caracter;
        int freq;
        entrada.read(&caracter, sizeof(caracter));
        entrada.read((char *)&freq, sizeof(freq));
        frecuencias[caracter] = freq;
    }

    priority_queue<Nodo *, vector<Nodo *>, Comparar> cola;
    for (auto par : frecuencias)
    {
        cola.push(new Nodo(par.first, par.second));
    }

    while (cola.size() > 1)
    {
        Nodo *izq = cola.top();
        cola.pop();
        Nodo *der = cola.top();
        cola.pop();
        cola.push(new Nodo(izq, der));
    }
    Nodo *raiz = cola.top();

    ofstream salida(archivoSalida, ios::binary);
    Nodo *actual = raiz;
    int caracteresEscritos = 0;
    unsigned char buffer;

    while (entrada.read((char *)&buffer, sizeof(buffer)) && caracteresEscritos < totalCaracteresOriginales)
    {
        for (int i = 7; i >= 0; i--)
        {
            int bit = (buffer >> i) & 1;

            if (bit == 0)
            {
                actual = actual->izq;
            }

            else
            {
                actual = actual->der;
            }

            if (!actual->izq && !actual->der)
            {
                salida.write(&actual->caracter, sizeof(actual->caracter));
                caracteresEscritos++;
                actual = raiz;
                if (caracteresEscritos == totalCaracteresOriginales)
                    break;
            }
        }
    }
}

int main()
{
    // string original = "GUIA DE FORMATO DE TESIS 2.0.pdf";
    // string original = "quijote.txt";
    // string original = "perrito_corriendo.jpg";

    // string original = "data.csv";
    // string comprimido = "data.huff";
    // string restaurado = "datahuffman.csv";

    // string original = "sqlite3.c";
    // string comprimido = "sqlite3.huff";
    // string restaurado = "sqlite3huffman.c";

    // string original = "perrito_corriendo.jpg";
    // string comprimido = "perrito_corriendo.huff";
    // string restaurado = "perrito_corriendo_restaurado.jpg";

    string original = "quijote.txt";
    string comprimido = "quijote.huff";
    string restaurado = "quijote_restaurado.txt";

    cout << "Comprimiendo...\n";
    comprimir(original, comprimido);

    cout << "Descomprimiendo...\n";
    descomprimir(comprimido, restaurado);

    return 0;
}