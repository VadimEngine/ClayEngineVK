// TODO make a simple static lib that in used in the demo here
// create a window and init graphics
// have a camera and render a triangle
// add imgui
// add 3d models loading
// create entity component system


#include <clay/gui/Window.h>
#include <clay/application/App.h>

int main() {
    App app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}