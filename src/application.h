#pragma once

#include "event/key_event.h"
#include "event/mouse_event.h"
#include "pch.h"
#include "renderer/renderer.h"
#include "window.h"
#include "event/window_event.h"
#include <chrono>

struct GLFWwindow;
class Application {
public:
    Application();

    ~Application();

    void onEvent(Event& e);

    void run();

    Window& getWindow() { return window_; };

private:
    bool onWindowClose(WindowCloseEvent& e);
    bool onWindowResize(WindowResizeEvent& e);
    bool onKeyPress(KeyPressedEvent& e);
    bool onKeyRelease(KeyReleasedEvent& e);
    bool onMouseMove(MouseMovedEvent& e);

    static Application* instance_;
    bool running_ = false;
    bool minimized_ = false;
    Window window_;
    yuubi::Renderer renderer_;
    yuubi::Camera camera_;

    float deltaTime_ = 0.0f;
    float averageFPS_ = 0.0f;
    ;
};
