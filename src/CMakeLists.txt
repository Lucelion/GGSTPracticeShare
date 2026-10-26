cmake_minimum_required(VERSION 3.20)
project(GGSTPracticeShare C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 유저 PC의 VC++ 재배포 패키지 누락 문제를 방지하기 위해 정적 런타임(/MT) 사용
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/minhook/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniz
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/nlohmann
)

set(SOURCES
    main.cpp
    game/game_memory.cpp
    core/config.cpp
    core/recording_data.cpp
    core/dx11_hook.cpp
    ui/ui.cpp

    # ImGui
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_win32.cpp
    vendor/imgui/backends/imgui_impl_dx11.cpp

    # MinHook
    vendor/minhook/src/buffer.c
    vendor/minhook/src/hook.c
    vendor/minhook/src/trampoline.c
    vendor/minhook/src/hde/hde64.c

    # miniz
    vendor/miniz/miniz.c
    vendor/miniz/miniz_tdef.c
    vendor/miniz/miniz_tinfl.c
)

add_library(main SHARED ${SOURCES})

target_link_libraries(main PRIVATE
    d3d11
    dxgi
    user32
    gdi32
    imm32
)

set_target_properties(main PROPERTIES
    OUTPUT_NAME "main"
    PREFIX ""
)

if (MSVC)
    target_compile_options(main PRIVATE /utf-8 /W3 /EHa)
endif()
