target_sources(
    ${PROJECT_NAME}
    PRIVATE
    "event/event.h"
    "event/key_event.h"
    "event/mouse_event.h"
    "event/window_event.h"
    "renderer/renderer.h"
    "renderer/renderer.cpp"
    "application.h"
    "application.cpp"
    "key_codes.h"
    "log.h"
    "log.cpp"
    "main.cpp"
    "mouse_codes.h"
    "window.h"
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
