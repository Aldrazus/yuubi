module;

#include <GLFW/glfw3.h>
#include <string>
#include <string_view>
#include <functional>

import Yuubi.Event;

export module Yuubi.Window;

export class Window {
public:
    using EventCallbackFn = std::function<void(Event&)>;

    Window(uint32_t width, uint32_t height, std::string_view title)
        : width_(width), height_(height), title_(title) {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window_ =
            glfwCreateWindow(width_, height_, title_.data(), nullptr, nullptr);
        glfwSetWindowUserPointer(window_, this);

        glfwSetWindowCloseCallback(window_, [](GLFWwindow* window) {
            Window* w =
                reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
            // send this to app
            WindowCloseEvent e;
            w->event_callback_(e);
        });

        glfwSetWindowSizeCallback(
            window_, [](GLFWwindow* window, int width, int height) {
                Window* w =
                    reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));

                WindowResizeEvent e{width, height};
                w->event_callback_(e);
            });
    }

    ~Window() {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }

    GLFWwindow* getWindow() const { return window_; }
    void onUpdate() { glfwPollEvents(); }

    // TODO: move this to constructor?
    void setEventCallback(EventCallbackFn fn) { event_callback_ = fn; }

private:
    uint32_t width_;
    uint32_t height_;
    std::string title_;
    GLFWwindow* window_ = nullptr;
    EventCallbackFn event_callback_;
};
