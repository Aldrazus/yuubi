#pragma once

#include "pch.h"
#include "event/event.h"

struct GLFWwindow;

class Window {
public:
    using EventCallbackFn = std::function<void(Event&)>;
    Window(uint32_t width, uint32_t height, std::string_view title);

    ~Window();

    static void processInput();

    void setEventCallback(EventCallbackFn);

    GLFWwindow* getWindow() const;

private:
    void initCallbacks() const;

    uint32_t width_;
    uint32_t height_;
    std::string title_;
    EventCallbackFn eventCallback_;
    GLFWwindow* window_ = nullptr;
};
