#include "application.h"
#include "core/log.h"

int main() {
    Log::Init();
    Application app;
    app.run();
}
