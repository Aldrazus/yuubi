#pragma once

#include "pch.h"
#include "renderer/renderer.h"
#include "window.h"
#include "event/window_event.h"

struct GLFWwindow;
class Application {
public:
    Application();

    ~Application();

    void OnEvent(Event& e);

    void Run();

    Window& GetWindow() { return window_; };

private:
    bool OnWindowClose(WindowCloseEvent& e);

    bool OnWindowResize(WindowResizeEvent& e);

    static Application* instance_;
    bool running_ = false;
    bool minimized_ = false;
    Window window_;
    yuubi::Renderer renderer_;
};
