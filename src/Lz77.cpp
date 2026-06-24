#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <algorithm>

using namespace std;

// Configuraciones de la ventana deslizante
const uint16_t MAX_VENTANA_BUSQUEDA = 4095; // Qué tan atrás podemos mirar
const uint8_t MAX_LONGITUD_COINCIDENCIA = 255; // Cuántos caracteres podemos copiar de golpe

// Nuestra tupla clásica de LZ77
struct Tupla {
    uint16_t distancia;
    uint8_t longitud;
    char siguiente_caracter;
};

// Función para buscar la repetición más larga en el "pasado"
Tupla buscarMejorCoincidencia(const vector<char>& datos, int posicionActual) {
    Tupla mejor = {0, 0, datos[posicionActual]};
    
    // Definir los límites de nuestra ventana de búsqueda (no podemos ir más atrás del inicio)
    int inicioBusqueda = max(0, posicionActual - MAX_VENTANA_BUSQUEDA);
    
    // Buscar en el texto pasado
    for (int j = inicioBusqueda; j < posicionActual; ++j) {
        int longitudActual = 0;
        
        // Mientras las letras coincidan y no nos pasemos del límite de lectura
        while (longitudActual < MAX_LONGITUD_COINCIDENCIA && 
               posicionActual + longitudActual < datos.size() &&
               datos[j + longitudActual] == datos[posicionActual + longitudActual]) {
            longitudActual++;
        }
        
        // Si encontramos una repetición más grande que la anterior guardada
        if (longitudActual > mejor.longitud) {
            mejor.distancia = posicionActual - j;
            mejor.longitud = longitudActual;
            
            // El siguiente caracter es el que rompe la coincidencia
            if (posicionActual + longitudActual < datos.size()) {
                mejor.siguiente_caracter = datos[posicionActual + longitudActual];
            } else {
                mejor.siguiente_caracter = '\0'; // Fin del archivo
            }
        }
    }
    
    return mejor;
}

// --- COMPRESIÓN REAL ---
void comprimirLZ77(const string& archivoEntrada, const string& archivoSalida) {
    ifstream entrada(archivoEntrada, ios::binary | ios::ate);
    if (!entrada) { cout << "Error leyendo entrada.\n"; return; }
    
    streamsize tamaño = entrada.tellg();
    entrada.seekg(0, ios::beg);
    
    // Cargar todo el archivo en memoria (para simplificar la búsqueda)
    vector<char> datos(tamaño);
    if (!entrada.read(datos.data(), tamaño)) {
        return;
    }
    
    ofstream salida(archivoSalida, ios::binary);
    
    if (!salida) { 
        cout << "Error creando salida.\n"; return; 
    }

    int i = 0;
    while (i < datos.size()) {
        Tupla tupla = buscarMejorCoincidencia(datos, i);
        
        // Escribir la tupla al disco duro (2 bytes + 1 byte + 1 byte = 4 bytes)
        salida.write(reinterpret_cast<const char*>(&tupla.distancia), sizeof(tupla.distancia));
        salida.write(reinterpret_cast<const char*>(&tupla.longitud), sizeof(tupla.longitud));
        salida.write(&tupla.siguiente_caracter, sizeof(tupla.siguiente_caracter));
        
        // Avanzamos la posición actual saltándonos lo que acabamos de comprimir + 1 (el nuevo caracter)
        i += tupla.longitud + 1;
    }
    cout << "Archivo comprimido con LZ77 exitosamente.\n";
}

// --- DESCOMPRESIÓN REAL ---
void descomprimirLZ77(const string& archivoEntrada, const string& archivoSalida) {
    ifstream entrada(archivoEntrada, ios::binary);
    if (!entrada) { cout << "Error leyendo archivo comprimido.\n"; return; }
    
    ofstream salida(archivoSalida, ios::binary);
    if (!salida) { cout << "Error creando salida.\n"; return; }

    vector<char> textoDescomprimido;
    Tupla tupla;

    // Leer el archivo comprimido tupla por tupla (saltando de 4 en 4 bytes)
    while (entrada.read(reinterpret_cast<char*>(&tupla.distancia), sizeof(tupla.distancia))) {
        entrada.read(reinterpret_cast<char*>(&tupla.longitud), sizeof(tupla.longitud));
        entrada.read(&tupla.siguiente_caracter, sizeof(tupla.siguiente_caracter));

        // Si la distancia es > 0, significa que debemos copiar del pasado
        if (tupla.distancia > 0) {
            int inicioCopia = textoDescomprimido.size() - tupla.distancia;
            for (int k = 0; k < tupla.longitud; ++k) {
                textoDescomprimido.push_back(textoDescomprimido[inicioCopia + k]);
            }
        }
        
        // Siempre añadimos el caracter nuevo al final
        if (tupla.siguiente_caracter != '\0' || entrada.peek() != EOF) {
            textoDescomprimido.push_back(tupla.siguiente_caracter);
        }
    }

    // Escribir todo el texto reconstruido al archivo
    salida.write(textoDescomprimido.data(), textoDescomprimido.size());
    cout << "Archivo descomprimido con LZ77 exitosamente.\n";
}

int main() {
    // string original = "GUIA DE FORMATO DE TESIS 2.0.pdf";
    // string original = "quijote.txt";
    // string original = "perrito_corriendo.jpg";
    
    // string original = "quijote.txt";
    // string comprimido = "quijote.lz77";
    // string restaurado = "quijote_lz77.txt";

    // string original = "data.csv";
    // string comprimido = "data.lz77";
    // string restaurado = "data_lz77.csv";

    // string original = "sqlite3.c";
    // string comprimido = "sqlite3.lz77";
    // string restaurado = "sqlite3_lz77.c";

    string original = "perrito_corriendo.jpg";
    string comprimido = "perrito_corriendo.lz77";
    string restaurado = "perrito_corriendo_lz77.jpg";

    cout << "Comprimiendo ... \n";
    comprimirLZ77(original, comprimido);
    
    cout << "Descomprimiendo...\n";
    descomprimirLZ77(comprimido, restaurado);
    
    return 0;
}