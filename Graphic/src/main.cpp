#include "app.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "Options:\n"
              << "  -w WIDTH     Window width (default: 1280)\n"
              << "  -h HEIGHT    Window height (default: 720)\n"
              << "  -s URL       Server WebSocket URL (default: ws://localhost:8080/ws)\n"
              << "  --help       Show this help message\n";
}

int main(int argc, char* argv[]) {
    cosmodrom::AppConfig config;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            config.screenWidth = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            config.screenHeight = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            config.serverUrl = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }
    }

    std::cout << "Cosmodrom 3D Visualizer\n";
    std::cout << "Server: " << config.serverUrl << "\n";
    std::cout << "Window: " << config.screenWidth << "x" << config.screenHeight << "\n";

    cosmodrom::Application app(config);
    app.run();

    return 0;
}
