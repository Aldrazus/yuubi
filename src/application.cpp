#include "application.h"
#include "pch.h"
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

Application* Application::instance_ = nullptr;

Application::Application() {
    if (instance_ != nullptr) {
        VKT_ERROR("Application already exists");
        exit(1);
    }
    instance_ = this;
    Log::Init();
    VKT_INFO("Starting application");

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(800, 600, "Vulkan Template", nullptr, nullptr);
}

Application::~Application() {
    glfwDestroyWindow(window_);
    glfwTerminate();
    instance_ = nullptr;
}

void Application::Run() {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::cout << extensionCount << " extensions supported\n";

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
    }
}
