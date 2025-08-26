#include <Application.h>
#include <iostream>
#include <GLFW/glfw3.h>

int main() {
    try {

        Application app("Real-time Rendering Engine");
        if (!app.initialize()) {
            std::cerr << "Failed to initialize application" << std::endl;
            return -1;
        }

        app.run();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
}
