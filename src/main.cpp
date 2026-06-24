#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <algorithm>

// CLI11
#include <CLI/CLI.hpp>

// FTXUI
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>

// Core
#include "core/Huffman.hpp"
#include "core/Lz77.hpp"
#include "core/Deflate.hpp"
#include "core/Gzip.hpp"
#include "core/Compressor.hpp"

using namespace ftxui;

struct TUIState {
    std::mutex mtx;
    float progress = 0.0f;
    std::string status_text = "Esperando... Selecciona un archivo y un algoritmo.";
    bool is_running = false;
};

void launchTUI() {
    auto screen = ScreenInteractive::Fullscreen();
    auto state = std::make_shared<TUIState>();

    std::string input_file = "quijote.txt";
    Component input_file_comp = Input(&input_file, "ruta/del/archivo.txt");

    int action_selected = 0;
    std::vector<std::string> action_entries = { "Comprimir", "Descomprimir" };
    Component action_radio = Radiobox(&action_entries, &action_selected);

    int algo_selected = 0;
    std::vector<std::string> algo_entries = { "deflate", "gzip", "huffman", "lz77" };
    Component algo_radio = Radiobox(&algo_entries, &algo_selected);

    auto start_button = Button("▷ Iniciar Operación [Enter]", [&, state] {
        std::string clean_input;
        {
            std::lock_guard<std::mutex> lock(state->mtx);
            if (state->is_running) return;
            
            clean_input = input_file;
            clean_input.erase(std::remove(clean_input.begin(), clean_input.end(), '\n'), clean_input.end());
            clean_input.erase(std::remove(clean_input.begin(), clean_input.end(), '\r'), clean_input.end());

            if (clean_input.empty()) {
                state->status_text = "Error: La ruta está vacía.";
                return;
            }

            state->is_running = true;
            state->progress = 0.0f;
            state->status_text = "Iniciando motor ZCore...";
        }

        std::thread([&screen, state, input_file_copy = clean_input, algo_idx = algo_selected, act_idx = action_selected]() {
            try {
                std::ifstream inFile(input_file_copy, std::ios::binary);
                if (!inFile) throw std::runtime_error("No se pudo abrir el archivo. ¿Existe la ruta?");

                inFile.seekg(0, std::ios::end);
                size_t total_size = inFile.tellg();
                inFile.seekg(0, std::ios::beg);

                std::string output_file;
                if (act_idx == 0) { // Comprimir
                    std::string ext = "";
                    if (algo_idx == 0) ext = ".df";
                    else if (algo_idx == 1) ext = ".gz";
                    else if (algo_idx == 2) ext = ".huf";
                    else if (algo_idx == 3) ext = ".l7";
                    output_file = input_file_copy + ext;
                } else { // Descomprimir
                    std::string base = input_file_copy;
                    if (base.size() >= 3 && (base.substr(base.size() - 3) == ".df" || base.substr(base.size() - 3) == ".gz" || base.substr(base.size() - 3) == ".l7")) {
                        base = base.substr(0, base.size() - 3);
                    } else if (base.size() >= 4 && base.substr(base.size() - 4) == ".huf") {
                        base = base.substr(0, base.size() - 4);
                    }
                    output_file = base + ".restaurado";
                }
                std::ofstream outFile(output_file, std::ios::binary);

                std::unique_ptr<Compressor> comp;
                if (algo_idx == 2) comp = std::make_unique<Huffman>();
                else if (algo_idx == 3) comp = std::make_unique<Lz77>();
                else if (algo_idx == 0) comp = std::make_unique<Deflate>();
                else if (algo_idx == 1) comp = std::make_unique<Gzip>();

                auto callback = [state, total_size, &screen](size_t processed) {
                    std::lock_guard<std::mutex> lock(state->mtx);
                    if (total_size > 0) {
                        state->progress = static_cast<float>(processed) / total_size;
                        if (state->progress > 1.0f) state->progress = 1.0f;
                    }
                    double mb = processed / (1024.0 * 1024.0);
                    state->status_text = "Procesando... " + std::to_string(mb).substr(0, 5) + " MB";
                    screen.PostEvent(Event::Custom);
                };

                auto start_t = std::chrono::high_resolution_clock::now();

                if (act_idx == 0) comp->compress(inFile, outFile, callback);
                else comp->decompress(inFile, outFile, callback);

                auto end_t = std::chrono::high_resolution_clock::now();
                auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end_t - start_t).count();

                std::lock_guard<std::mutex> lock(state->mtx);
                state->progress = 1.0f;
                state->status_text = "¡Éxito! Archivo guardado. Tiempo: " + std::to_string(dur) + " ms";
                state->is_running = false;
                screen.PostEvent(Event::Custom);

            } catch(const std::exception& e) {
                std::lock_guard<std::mutex> lock(state->mtx);
                state->status_text = std::string("Error Crítico: ") + e.what();
                state->is_running = false;
                screen.PostEvent(Event::Custom);
            }
        }).detach();

    }, ButtonOption::Animated(Color::Green));

    auto quit_button = Button("↳ Salir [Esc]", [&, state] { 
        std::lock_guard<std::mutex> lock(state->mtx);
        if (!state->is_running) screen.ExitLoopClosure()(); 
    }, ButtonOption::Animated(Color::GrayDark));

    auto container = Container::Vertical({
        input_file_comp,
        Container::Horizontal({ action_radio, algo_radio }),
        Container::Horizontal({ start_button, quit_button })
    });

    auto component = CatchEvent(container, [&, state](Event event) {
        if (event == Event::Character('q') || event == Event::Escape) {
            std::lock_guard<std::mutex> lock(state->mtx);
            if (!state->is_running) screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    auto renderer = Renderer(component, [&, state] {
        std::lock_guard<std::mutex> lock(state->mtx);
        
        auto left_col = vbox({
            text(" ARCHIVO DE ENTRADA") | dim,
            hbox({ text(" 📁 "), input_file_comp->Render() | flex }) | borderRounded,
            separatorEmpty(),
            text(" ACCIÓN") | dim,
            action_radio->Render()
        });

        auto right_col = vbox({
            text(" MOTOR ALGORÍTMICO") | dim,
            algo_radio->Render()
        });

        auto operacion_content = vbox({
            separatorEmpty(), // Padding superior interno
            hbox({
                text("  "), // Margen izquierdo
                left_col | flex | size(WIDTH, GREATER_THAN, 35),
                text("     "), // Espaciado central ancho
                right_col | size(WIDTH, GREATER_THAN, 25),
                text("  ")  // Margen derecho
            }),
            separatorEmpty(),
            separatorLight(),
            separatorEmpty(),
            hbox({
                text("  "),
                start_button->Render() | flex,
                text("    "),
                quit_button->Render() | flex,
                text("  ")
            }),
            separatorEmpty(),
            separatorLight(),
            separatorEmpty(),
            text("   🕒 " + state->status_text) | (state->is_running ? color(Color::Yellow) : dim),
            separatorEmpty() // Padding inferior interno
        });

        auto operacion_window = window(text(" OPERACIÓN "), operacion_content);

        auto main_content = vbox({
            text(" ⧉ ZCore v0.0.1 ") | bold | color(Color::Cyan) | center,
            separatorEmpty(),
            operacion_window
        });

        if (state->is_running && state->progress < 1.0f) {
            main_content = vbox({
                main_content,
                separatorEmpty(),
                gauge(state->progress) | color(Color::Cyan)
            });
        }

        // Envolvemos todo en un contenedor con márgenes generosos
        auto padded_content = hbox({
            text("   "), // Margen lateral global izquierdo
            main_content | flex,
            text("   ")  // Margen lateral global derecho
        });

        return padded_content | borderEmpty | borderDouble | center;
    });

    screen.Loop(renderer);
}

int main(int argc, char** argv) {
    CLI::App app{"ZCore v1.0"};

    bool is_compress = false;
    bool is_decompress = false;
    bool tui_mode = false;
    bool verbose = false;
    std::string algorithm = "deflate";
    std::string input_file = "";
    std::string output_file = "";

    app.add_flag("-c,--compress", is_compress, "Comprime el archivo de entrada");
    app.add_flag("-d,--decompress", is_decompress, "Descomprime el archivo de entrada");
    app.add_option("-a,--algo", algorithm, "Selecciona el algoritmo: huffman, lz77, deflate, gzip")
       ->check(CLI::IsMember({"huffman", "lz77", "deflate", "gzip"}));
    app.add_option("-i,--input", input_file, "Archivo de entrada a procesar");
    app.add_option("-o,--output", output_file, "Archivo de salida resultante");
    app.add_flag("-v,--verbose", verbose, "Muestra estadísticas detalladas");
    app.add_flag("--tui", tui_mode, "Inicia la Interfaz Gráfica (Menú interactivo)");

    CLI11_PARSE(app, argc, argv);

    if (tui_mode || argc == 1) {
        launchTUI();
        return 0;
    }

    try {
        if (!is_compress && !is_decompress) throw std::runtime_error("Falta acción: --compress o --decompress");
        if (is_compress && is_decompress) throw std::runtime_error("Conflicto: No puedes comprimir y descomprimir a la vez.");
        if (input_file.empty()) throw std::runtime_error("Falta archivo de entrada (-i).");

        if (output_file.empty()) {
            if (is_compress) {
                if (algorithm == "deflate") output_file = input_file + ".df";
                else if (algorithm == "gzip") output_file = input_file + ".gz";
                else if (algorithm == "huffman") output_file = input_file + ".huf";
                else if (algorithm == "lz77") output_file = input_file + ".l7";
            } else {
                std::string base = input_file;
                if (base.size() >= 3 && (base.substr(base.size() - 3) == ".df" || base.substr(base.size() - 3) == ".gz" || base.substr(base.size() - 3) == ".l7")) {
                    base = base.substr(0, base.size() - 3);
                } else if (base.size() >= 4 && base.substr(base.size() - 4) == ".huf") {
                    base = base.substr(0, base.size() - 4);
                }
                output_file = base + ".restaurado";
            }
        }

        std::unique_ptr<Compressor> compressor;
        if (algorithm == "huffman") compressor = std::make_unique<Huffman>();
        else if (algorithm == "lz77") compressor = std::make_unique<Lz77>();
        else if (algorithm == "deflate") compressor = std::make_unique<Deflate>();
        else if (algorithm == "gzip") compressor = std::make_unique<Gzip>();

        std::ifstream inFile(input_file, std::ios::binary);
        if (!inFile) throw std::runtime_error("Archivo no encontrado: " + input_file);
        
        std::ofstream outFile(output_file, std::ios::binary);
        if (!outFile) throw std::runtime_error("No se pudo crear: " + output_file);

        if (verbose) std::cout << "\033[32m[+] ZCore CLI Iniciado...\033[0m\n";

        auto start = std::chrono::high_resolution_clock::now();
        if (is_compress) compressor->compress(inFile, outFile, nullptr);
        else compressor->decompress(inFile, outFile, nullptr);
        auto end = std::chrono::high_resolution_clock::now();

        if (verbose) {
            auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            std::cout << "\033[32m[✓] Completado (" << dur << " ms).\033[0m\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "\033[31m[Error] " << e.what() << "\033[0m\n";
        return 1;
    }
    return 0;
}
