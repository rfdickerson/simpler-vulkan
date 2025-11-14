#include <exception>
#include <iostream>

#include "terrain_application.hpp"

int main() {
    try {
        TerrainApplication app;
        app.initialize();
        app.run();
        app.shutdown();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
