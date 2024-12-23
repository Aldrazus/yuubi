#include "window.h"

#include "event/window_event.h"
#include "event/key_event.h"
#include "event/mouse_event.h"
#include "imgui.h"
#include "pch.h"

#include <GLFW/glfw3.h>

Window::Window(uint32_t width, uint32_t height, std::string_view title) :
    width_(width), height_(height), title_(title) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(width_, height_, title_.data(), nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    initCallbacks();
}

void Window::initCallbacks() const {
    glfwSetWindowCloseCallback(window_, [](GLFWwindow* window) {
        const Window* w = static_cast<Window*>(glfwGetWindowUserPointer(window));
        // send this to app
        WindowCloseEvent e;
        w->eventCallback_(e);
    });

    glfwSetWindowSizeCallback(window_, [](GLFWwindow* window, int width, int height) {
        const auto w = static_cast<Window*>(glfwGetWindowUserPointer(window));

        WindowResizeEvent e{width, height};
        w->eventCallback_(e);
    });

    glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (auto& io = ImGui::GetIO(); io.WantCaptureKeyboard) {
            return;
        }

        const auto w = static_cast<Window*>(glfwGetWindowUserPointer(window));

        switch (action) {
            case GLFW_PRESS: {
                KeyPressedEvent event(key, false);
                w->eventCallback_(event);
                break;
            }
            case GLFW_REPEAT: {
                KeyPressedEvent event(key, true);
                w->eventCallback_(event);
                break;
            }
            case GLFW_RELEASE: {
                KeyReleasedEvent event(key);
                w->eventCallback_(event);
                break;
            }
        }
    });

    glfwSetCharCallback(window_, [](GLFWwindow* window, unsigned int codepoint) {
        auto& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard) {
            return;
        }

        Window* w = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        KeyTypedEvent event(codepoint);
        w->eventCallback_(event);
    });

    glfwSetCursorPosCallback(window_, [](GLFWwindow* window, double xpos, double ypos) {
        if (const auto& io = ImGui::GetIO(); io.WantCaptureMouse) {
            return;
        }

        auto w = static_cast<Window*>(glfwGetWindowUserPointer(window));
        MouseMovedEvent event(xpos, ypos);
        w->eventCallback_(event);
    });

    glfwSetMouseButtonCallback(window_, [](GLFWwindow* window, int button, int action, int mods) {
        auto& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }

        const Window* w = static_cast<Window*>(glfwGetWindowUserPointer(window));
        switch (action) {
            case GLFW_PRESS: {
                MouseButtonPressedEvent event(button);
                w->eventCallback_(event);
                break;
            }
            case GLFW_RELEASE: {
                MouseButtonReleasedEvent event(button);
                w->eventCallback_(event);
                break;
            }
        }
    });

    glfwSetScrollCallback(window_, [](GLFWwindow* window, double xoffset, double yoffset) {
        if (const auto& io = ImGui::GetIO(); io.WantCaptureMouse) {
            return;
        }

        const Window* w = static_cast<Window*>(glfwGetWindowUserPointer(window));
        MouseScrollEvent event(xoffset, yoffset);
        w->eventCallback_(event);
    });
}

// TODO: move this to constructor?
void Window::setEventCallback(EventCallbackFn fn) { eventCallback_ = fn; }

GLFWwindow* Window::getWindow() const { return window_; }

Window::~Window() {
    glfwDestroyWindow(window_);
    glfwTerminate();
}

void Window::processInput() { glfwPollEvents(); }
