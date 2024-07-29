target_sources(
    ${PROJECT_NAME}
    PRIVATE
    "core/io/file.cpp"
    "core/log.cpp"
    "renderer/camera.cpp"
    "renderer/descriptor_allocator.cpp"
    "renderer/descriptor_layout_builder.cpp"
    "renderer/device.cpp"
    "renderer/instance.cpp"
    "renderer/pipeline_builder.cpp"
    "renderer/renderer.cpp"
    "renderer/viewport.cpp"
    "renderer/vulkan_usage.cpp"
    "renderer/vma/allocator.cpp"
    "renderer/vma/image.cpp"
    "renderer/vma/buffer.cpp"
    "application.cpp"
    "main.cpp"
    "window.cpp"
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    .
)

target_precompile_headers(
    ${PROJECT_NAME}
    PRIVATE
    "pch.h"
)

if (MSVC)
    target_compile_options(
        ${PROJECT_NAME}
        PUBLIC
        # debug
        "$<$<CONFIG:Debug>:/W4>"
        "$<$<CONFIG:Debug>:/Zi>"

        # rel w/ debug info (for profiling)
        "$<$<CONFIG:RelWithDebInfo>:/Zi>"
        "$<$<CONFIG:RelWithDebInfo>:/O2>"

        # release
        "$<$<CONFIG:Release>:/O2>"
    )

    target_link_options(
        ${PROJECT_NAME}
        PUBLIC
        # debug
        "$<$<CONFIG:Debug>:/DEBUG:FULL>"
        "$<$<CONFIG:Debug>:/INCREMENTAL>"

        # rel w/ debug info (for profiling)
        "$<$<CONFIG:RelWithDebInfo>:/DEBUG:FULL>"
        "$<$<CONFIG:RelWithDebInfo>:/PROFILE>"
        # already implied
        "$<$<CONFIG:RelWithDebInfo>:/DEBUG:FULL>"
        "$<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO>"

        # release
        "$<$<CONFIG:Release>:/INCREMENTAL:NO>"
    )
else()
    target_compile_options(
        ${PROJECT_NAME}
        PUBLIC
        # debug
        "$<$<CONFIG:Debug>:-Wall>"
        "$<$<CONFIG:Debug>:-Wextra>"
        "$<$<CONFIG:Debug>:-g>"
        "$<$<CONFIG:Debug>:-gcodeview>"
        "$<$<CONFIG:Debug>:-fcolor-diagnostics>"
        "$<$<CONFIG:Debug>:-fansi-escape-codes>"

        # rel w/ debug info (for profiling)
        "$<$<CONFIG:RelWithDebInfo>:-O3>"
        "$<$<CONFIG:RelWithDebInfo>:-g>"
        "$<$<CONFIG:RelWithDebInfo>:-gcodeview>"

        # release
        "$<$<CONFIG:Release>:-O3>"
    )

    if (WIN32)
        # On Windows, clang's lld-link supports the same flags as link.exe
        target_link_options(
            ${PROJECT_NAME}
            PUBLIC
            # debug
            "$<$<CONFIG:Debug>:LINKER:/DEBUG:FULL>"
            "$<$<CONFIG:Debug>:LINKER:/INCREMENTAL>"

            # rel w/ debug info (for profiling)
            "$<$<CONFIG:RelWithDebInfo>:LINKER:/DEBUG:FULL>"
            "$<$<CONFIG:RelWithDebInfo>:LINKER:/PROFILE>"
            # already implied
            "$<$<CONFIG:RelWithDebInfo>:LINKER:/DEBUG:FULL>"
            "$<$<CONFIG:RelWithDebInfo>:LINKER:/INCREMENTAL:NO>"

            # release
            "$<$<CONFIG:Release>:LINKER:/INCREMENTAL:NO>"
        )
    else()
        target_link_options(
            ${PROJECT_NAME}
            PUBLIC
            # debug
            "$<$<CONFIG:Debug>:LINKER:--pdb=>"

            # rel w/ debug info (for profiling)
            "$<$<CONFIG:RelWithDebInfo>:LINKER:--pdb=>"
        )
    endif()
endif()

target_compile_definitions(
    ${PROJECT_NAME}
    PUBLIC
    $<$<CONFIG:Debug>:UB_DEBUG>
)
