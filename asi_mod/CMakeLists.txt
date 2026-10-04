cmake_minimum_required(VERSION 3.24)
project(DarksidersGenesisASIMod VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)

FetchContent_Declare(
  minhook
  GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
  GIT_TAG v1.3.4
)
FetchContent_MakeAvailable(minhook)

if(TARGET minhook)
  set(DG_MINHOOK_TARGET minhook)
else()
  message(STATUS "MinHook CMake target not exported; using local fallback target")
  add_library(dg_minhook STATIC
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
  )
  target_include_directories(dg_minhook
    PUBLIC ${minhook_SOURCE_DIR}/include
    PRIVATE ${minhook_SOURCE_DIR}/src ${minhook_SOURCE_DIR}/src/hde
  )
  set(DG_MINHOOK_TARGET dg_minhook)
endif()

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.8
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_static STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui_static PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui_static PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(imgui_static PUBLIC d3d11 dxgi user32 gdi32 imm32 dwmapi)

add_library(DarksidersGenesisMod SHARED
  src/DarksidersGenesisMod.cpp
)
target_compile_definitions(DarksidersGenesisMod PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_include_directories(DarksidersGenesisMod PRIVATE ${minhook_SOURCE_DIR}/include)
target_link_libraries(DarksidersGenesisMod PRIVATE
  imgui_static
  ${DG_MINHOOK_TARGET}
  d3d11
  dxgi
  user32
  gdi32
  imm32
  dwmapi
)
set_target_properties(DarksidersGenesisMod PROPERTIES
  PREFIX ""
  SUFFIX ".asi"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/out"
)

add_library(dxgi_proxy SHARED
  src/dxgi_proxy.cpp
)
target_compile_definitions(dxgi_proxy PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(dxgi_proxy PRIVATE user32)
set_target_properties(dxgi_proxy PROPERTIES
  OUTPUT_NAME "dxgi"
  PREFIX ""
  SUFFIX ".dll"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/out"
)

if(MSVC)
  target_compile_options(DarksidersGenesisMod PRIVATE /W4 /EHsc /permissive-)
  target_compile_options(dxgi_proxy PRIVATE /W4 /EHsc /permissive-)
endif()
