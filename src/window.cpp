#include "window.h"

#include "event/window_event.h"
#include "pch.h"
#include <GLFW/glfw3.h>

Window::Window(uint32_t width, uint32_t height, std::string_view title)
    : width_(width), height_(height), title_(title) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window_ =
        glfwCreateWindow(width_, height_, title_.data(), nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);
    InitCallbacks();
}

void Window::InitCallbacks() {
    glfwSetWindowCloseCallback(window_, [](GLFWwindow* window) {
        Window* w = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        // send this to app
        WindowCloseEvent e;
        w->event_callback_(e);
    });

    glfwSetWindowSizeCallback(window_, [](GLFWwindow* window, int width,
                                          int height) {
        Window* w = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));

        WindowResizeEvent e{width, height};
        w->event_callback_(e);
    });
}

// TODO: move this to constructor?
void Window::SetEventCallback(EventCallbackFn fn) { event_callback_ = fn; }

Window::~Window() {
    glfwDestroyWindow(window_);
    glfwTerminate();
}

void Window::OnUpdate() { glfwPollEvents(); }
