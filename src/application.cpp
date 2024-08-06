#include "application.h"
#include <algorithm>
#include <chrono>
#include "event/event.h"
#include "event/key_event.h"
#include "event/mouse_event.h"
#include "key_codes.h"
#include "pch.h"

#define UB_BIND_EVENT_FN(fn)                                    \
    [this](auto&&... args) -> decltype(auto) {                  \
        return this->fn(std::forward<decltype(args)>(args)...); \
    }
#if 0
#define UB_BIND_EVENT_FN(fn) [this](Event& e) { fn(e); }
#endif

Application* Application::instance_ = nullptr;

Application::Application()
    : window_(800, 600, "Yuubi"),
      renderer_(window_),
      // TODO: initialize camera with aspect ratio calculated using viewport
      camera_(glm::vec3(2.0f, 0.0f, 2.0f), glm::vec3(0.0f), 0.0f, 0.0f) {
    if (instance_ != nullptr) {
        UB_ERROR("Application already exists");
        exit(1);
    }
    instance_ = this;
    window_.setEventCallback(UB_BIND_EVENT_FN(onEvent));
    previousFrameTime = std::chrono::high_resolution_clock::now();
    UB_INFO("Starting application");
}

Application::~Application() {}

bool Application::onWindowClose(WindowCloseEvent& e) {
    running_ = false;
    UB_INFO("Closing application");
    return true;
}

bool Application::onWindowResize(WindowResizeEvent& e) {
    if (e.getWidth() == 0 && e.getHeight() == 0) {
        minimized_ = true;
        return false;
    }
    minimized_ = false;
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

    const float sensitivity = 100.0f;

    camera_.yaw += deltaX * deltaTime * sensitivity;

    camera_.pitch -= deltaY * deltaTime * sensitivity;
    camera_.pitch = std::clamp(camera_.pitch, -89.0f, 89.0f);

    oldX = e.xPos;
    oldY = e.yPos;

    return true;
}

void Application::onEvent(Event& e) {
    EventDispatcher dispatcher(e);
    dispatcher.dispatch<WindowCloseEvent>(
        UB_BIND_EVENT_FN(Application::onWindowClose)
    );
    dispatcher.dispatch<WindowResizeEvent>(
        UB_BIND_EVENT_FN(Application::onWindowResize)
    );
    dispatcher.dispatch<KeyPressedEvent>(
        UB_BIND_EVENT_FN(Application::onKeyPress)
    );
    dispatcher.dispatch<KeyReleasedEvent>(
        UB_BIND_EVENT_FN(Application::onKeyRelease)
    );
    dispatcher.dispatch<MouseMovedEvent>(
        UB_BIND_EVENT_FN(Application::onMouseMove)
    );
}

void Application::run() {
    running_ = true;
    UB_INFO("Running application");
    while (running_) {
        currentFrameTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsedTime =
            currentFrameTime - previousFrameTime;
        deltaTime = elapsedTime.count();
        previousFrameTime = currentFrameTime;

        window_.onUpdate();
        camera_.updatePosition(deltaTime);
        renderer_.draw(camera_);
    }
}
