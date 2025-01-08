#include <print>
#include "application.h"
#include "core/log.h"

int main(int argc, const char** argv) {
    Log::Init();
    if (argc != 2) {
        std::println("Usage: {} [filepath].gltf", argv[0]);
    }
    Application app(argv[1]);
    app.run();
}
