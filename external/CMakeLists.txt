set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    VULKAN_HPP_NO_CONSTRUCTORS
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
    NOMINMAX
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLFW_INCLUDE_NONE
)

# VulkanMemoryAllocator
add_subdirectory(VulkanMemoryAllocator)
target_link_libraries(${PROJECT_NAME} GPUOpen::VulkanMemoryAllocator)

# glfw
add_subdirectory(glfw)
target_link_libraries(${PROJECT_NAME} glfw)

# glm
add_subdirectory(glm)
target_link_libraries(${PROJECT_NAME} glm)

# Dear ImGui
add_library(imgui)
target_sources(imgui
    PRIVATE
    "imgui/imgui.h"
    "imgui/imgui.cpp"
    
    "imgui/imgui_demo.cpp"
    "imgui/imgui_draw.cpp"
    "imgui/imgui_widgets.cpp"
    "imgui/imgui_tables.cpp"
    "imgui/backends/imgui_impl_vulkan.cpp"
    "imgui/backends/imgui_impl_glfw.cpp"
    )
target_include_directories(imgui PUBLIC "imgui" "imgui/backends")
target_link_libraries(imgui PRIVATE Vulkan::Vulkan glfw)
target_link_libraries(${PROJECT_NAME} imgui)

# stb-image
add_subdirectory(stb_image)
target_link_libraries(${PROJECT_NAME} stb_image)

# spdlog
add_subdirectory(spdlog)
target_link_libraries(${PROJECT_NAME} spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)

# fastgltf
add_subdirectory(fastgltf)
target_link_libraries(${PROJECT_NAME} fastgltf)

# MikkTSpace
add_subdirectory(mikktspace)
target_link_libraries(${PROJECT_NAME} MikkTSpace)
