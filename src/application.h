#pragma once

#include "event/key_event.h"
#include "event/mouse_event.h"
#include "pch.h"
#include "renderer/renderer.h"
#include "window.h"
#include "event/window_event.h"
#include <chrono>

struct GLFWwindow;

struct AppState {
    bool isMinimized = false;
    float averageFPS;
    bool isCameraRotatable_ = false;
};

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
    bool onMouseButtonPressed(MouseButtonPressedEvent& e);
    bool onMouseButtonReleased(MouseButtonReleasedEvent& e);

    static Application* instance_;
    AppState state_;
    bool running_ = false;
    Window window_;
    yuubi::Renderer renderer_;
    yuubi::Camera camera_;

    float deltaTime_ = 0.0f;
};
