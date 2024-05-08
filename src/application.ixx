module;

#include <print>

#define UB_BIND_EVENT_FN(fn)                                    \
    [this](auto&&... args) -> decltype(auto) {                  \
        return this->fn(std::forward<decltype(args)>(args)...); \
    }

import Yuubi.Window;
import Yuubi.Event;

export module Yuubi.Application;

export class Application {
public:
    Application() : window_(800, 600, "Yuubi") {
        if (instance_ != nullptr) {
            std::println("Application already exists");
        }
        instance_ = this;
        window_.setEventCallback(UB_BIND_EVENT_FN(onEvent));
    }

    ~Application() {

    }

    void run() {
        running_ = true;
        while (running_) {
            window_.onUpdate();
        }
    }

private:
    bool onWindowResize(WindowResizeEvent& e) {
        if (e.getWidth() == 0 && e.getHeight() == 0) {
            minimized_ = true;
            return false;
        }
        minimized_ = false;
        // TODO: let renderer resize
        // renderer_.resize();
        return false;
    }

    bool onWindowClose(WindowCloseEvent& e) {
        running_ = false;
        std::println("Closing application");
        return true;
    }

    void onEvent(Event& e) {
        EventDispatcher dispatcher(e);
        dispatcher.dispatch<WindowCloseEvent>(
            UB_BIND_EVENT_FN(Application::onWindowClose));
        dispatcher.dispatch<WindowResizeEvent>(
            UB_BIND_EVENT_FN(Application::onWindowResize));
    }



    static Application* instance_;
    bool running_ = false;
    bool minimized_ = false;
    Window window_;
};

Application* Application::instance_ = nullptr;
