#include <print>
#include "application.h"
#include "core/log.h"

int main() {
    std::println("Loading...");
    Log::Init();
    Application app;
    app.run();
}
