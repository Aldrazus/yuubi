#include "application.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>
#include "event/event.h"
#include "event/key_event.h"
#include "event/mouse_event.h"
#include "key_codes.h"
#include "pch.h"
#include "renderer/camera.h"
#include <GLFW/glfw3.h>

#define UB_BIND_EVENT_FN(fn) \
    [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }
#if 0
#define UB_BIND_EVENT_FN(fn) [this](Event& e) { fn(e); }
#endif

Application* Application::instance_ = nullptr;

Application::Application() :
    window_(1600, 900, "Yuubi"), renderer_(window_),
    // TODO: initialize camera with aspect ratio calculated using viewport
    camera_(glm::vec3(2.0f, 0.0f, 2.0f), glm::vec3(0.0f), 0.0f, 0.0f,
            static_cast<float>(1600) / static_cast<float>(900)) {
    if (instance_ != nullptr) {
        UB_ERROR("Application already exists");
        exit(1);
    }
    instance_ = this;
    window_.setEventCallback(UB_BIND_EVENT_FN(onEvent));
    UB_INFO("Starting application");
}

Application::~Application() = default;

bool Application::onWindowClose(WindowCloseEvent& e) {
    running_ = false;
    UB_INFO("Closing application");
    return true;
}

bool Application::onWindowResize(WindowResizeEvent& e) {
    if (e.getWidth() == 0 && e.getHeight() == 0) {
        state_.isMinimized = true;
        return false;
    }
    state_.isMinimized = false;
    camera_ = yuubi::Camera(
            glm::vec3(2.0f, 0.0f, 2.0f), glm::vec3(0.0f), 0.0f, 0.0f,
            static_cast<float>(e.getWidth()) / static_cast<float>(e.getHeight())
    );
    // TODO: let renderer resize
    // renderer_.resize();
    return false;
}

bool Application::onKeyPress(KeyPressedEvent& e) {
    switch (e.keyCode) {
        case Key::W: {
            camera_.velocity.z = -1;
            break;
        }
        case Key::A: {
            camera_.velocity.x = -1;
            break;
        }
        case Key::S: {
            camera_.velocity.z = 1;
            break;
        }
        case Key::D: {
            camera_.velocity.x = 1;
        }
    }
    return true;
}

bool Application::onKeyRelease(KeyReleasedEvent& e) {
    // TODO: fix bug where camera stops when opposite keys are held and one is
    // released
    switch (e.keyCode) {
        case Key::Escape: {
            state_.isLocked = false;
            glfwSetInputMode(window_.getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        case Key::W: {
            camera_.velocity.z = 0;
            break;
        }
        case Key::A: {
            camera_.velocity.x = 0;
            break;
        }
        case Key::S: {
            camera_.velocity.z = 0;
            break;
        }
        case Key::D: {
            camera_.velocity.x = 0;
        }
    }
    return true;
}

bool Application::onMouseMove(MouseMovedEvent& e) {
    static double oldX = e.xPos;
    static double oldY = e.yPos;

    const double deltaX = e.xPos - oldX;
    const double deltaY = e.yPos - oldY;

    constexpr float sensitivity = 1.0f;

    camera_.yaw += deltaX * sensitivity;

    camera_.pitch -= deltaY * sensitivity;
    camera_.pitch = std::clamp(camera_.pitch, -89.0f, 89.0f);

    oldX = e.xPos;
    oldY = e.yPos;

    return true;
}

bool Application::onMouseButtonPressed(MouseButtonPressedEvent& e) {
    state_.isLocked = true;
    glfwSetInputMode(window_.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    return true;
}

void Application::onEvent(Event& e) {
    EventDispatcher dispatcher(e);
    dispatcher.dispatch<WindowCloseEvent>(UB_BIND_EVENT_FN(Application::onWindowClose));
    dispatcher.dispatch<WindowResizeEvent>(UB_BIND_EVENT_FN(Application::onWindowResize));
    dispatcher.dispatch<KeyPressedEvent>(UB_BIND_EVENT_FN(Application::onKeyPress));
    dispatcher.dispatch<KeyReleasedEvent>(UB_BIND_EVENT_FN(Application::onKeyRelease));
    dispatcher.dispatch<MouseMovedEvent>(UB_BIND_EVENT_FN(Application::onMouseMove));
    dispatcher.dispatch<MouseButtonPressedEvent>(UB_BIND_EVENT_FN(Application::onMouseButtonPressed));
}

void Application::run() {
    running_ = true;
    UB_INFO("Running application");

    // Game loop based on Glenn Fiedler's "Fix Your Timestep!" blog post:
    // https://gafferongames.com/post/fix_your_timestep/

    constexpr float simulationFramesPerSecond = 60.0f;
    constexpr float fixedTimestep = 1.0f / simulationFramesPerSecond;

    std::chrono::time_point<std::chrono::high_resolution_clock> previousFrameTime =
            std::chrono::high_resolution_clock::now();

    float accumulator = 0.0f;

    while (running_) {
        // 0. Manage time and frame rate.
        std::chrono::time_point<std::chrono::high_resolution_clock> currentFrameTime =
                std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> frameTime = currentFrameTime - previousFrameTime;
        previousFrameTime = currentFrameTime;
        deltaTime_ = frameTime.count();
        accumulator += deltaTime_;

        // Calculate exponential moving average for FPS.
        float currentFPS = 1.0f / deltaTime_;
        if (currentFPS == std::numeric_limits<float>::infinity()) {
            currentFPS = 0;
        }
        state_.averageFPS = std::lerp(state_.averageFPS, currentFPS, 0.1f);

        // 1. Handle input.
        window_.processInput();

        while (accumulator >= fixedTimestep) {
            // 2. Update game logic.
            // TODO: Use fixed-rate updates for physics.
            // Use per-frame variable-rate updates for input and rendering.
            camera_.updatePosition(fixedTimestep);

            accumulator -= fixedTimestep;
        }

        // 3. Render.
        renderer_.draw(camera_, state_);

        // Limit frame rate.
        {
            const auto now = std::chrono::high_resolution_clock::now();
            const auto frameTime = std::chrono::duration<float>(now - previousFrameTime).count();
            if (fixedTimestep > frameTime) {
                std::this_thread::sleep_for(
                        std::chrono::milliseconds(static_cast<int>((fixedTimestep - frameTime) * 1000.0))
                );
            }
        }
    }
}
