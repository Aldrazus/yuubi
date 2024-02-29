#include "application.h"
#include "event/event.h"
#include "pch.h"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#define UB_BIND_EVENT_FN(fn)                                    \
    [this](auto&&... args) -> decltype(auto) {                  \
        return this->fn(std::forward<decltype(args)>(args)...); \
    }
#if 0
#define UB_BIND_EVENT_FN(fn) [this](Event& e) { fn(e); }
#endif

Application* Application::instance_ = nullptr;

Application::Application() : window_(800, 600, "Yuubi"), renderer_(window_) {
    if (instance_ != nullptr) {
        UB_ERROR("Application already exists");
        exit(1);
    }
    instance_ = this;
    window_.SetEventCallback(UB_BIND_EVENT_FN(OnEvent));
    UB_INFO("Starting application");
}

Application::~Application() {}

bool Application::OnWindowClose(WindowCloseEvent& e) {
    running_ = false;
    UB_INFO("Closing application");
    return true;
}

bool Application::OnWindowResize(WindowResizeEvent& e) {
    if (e.getWidth() == 0 && e.getHeight() == 0) {
        minimized_ = true;
        return false;
    }
    minimized_ = false;
    // TODO: let renderer resize
    return false;
}

void Application::OnEvent(Event& e) {
    EventDispatcher dispatcher(e);
    dispatcher.dispatch<WindowCloseEvent>(
        UB_BIND_EVENT_FN(Application::OnWindowClose));
    dispatcher.dispatch<WindowResizeEvent>(
        UB_BIND_EVENT_FN(Application::OnWindowResize));
}

void Application::Run() {
    running_ = true;
    UB_INFO("Running application");
    while (running_) {
        window_.OnUpdate();
        renderer_.drawFrame();
    }
}
