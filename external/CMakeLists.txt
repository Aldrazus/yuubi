set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    VULKAN_HPP_NO_CONSTRUCTORS
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
    NOMINMAX
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLFW_INCLUDE_NONE
)

# VulkanMemoryAllocator
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Headers GPUOpen::VulkanMemoryAllocator)

# glfw
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# glm
find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)

# Dear ImGui
find_package(imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)

find_package(implot CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE implot::implot)

# stb-image
find_package(Stb REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})

# spdlog
find_package(spdlog CONFIG REQUIRED)
# TODO: remove last bits
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)

# fastgltf
find_package(fastgltf)
target_link_libraries(${PROJECT_NAME} PRIVATE fastgltf::fastgltf)

# MikkTSpace
find_package(mikktspace)
target_link_libraries(${PROJECT_NAME} PRIVATE mikktspace::mikktspace)
