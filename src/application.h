#pragma once

struct GLFWwindow;
class Application {
   public:
    Application();
    ~Application();

    void Run();

   private:
    static Application* instance_;
    GLFWwindow* window_ = nullptr;
};
