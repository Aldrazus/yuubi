#include "application.h"
#include "log.h"

int main() {
    Log::Init();
    Application app;
    app.Run();
}
