cmake_minimum_required(VERSION 3.16)
project(SimuladorSJF CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Dependencias externas: GLFW (ventana/entrada) y Dear ImGui (GUI inmediata)
# Se descargan automáticamente la primera vez que se configura el proyecto.
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.9
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

find_package(OpenGL REQUIRED)

# ---------------------------------------------------------------------------
# Fuentes propias del simulador
# ---------------------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/Process.cpp
    src/Scheduler.cpp
    src/Simulation.cpp
    src/Gui.cpp
)

add_executable(simulador_sjf ${SOURCES})
target_include_directories(simulador_sjf PRIVATE src)
target_link_libraries(simulador_sjf PRIVATE imgui OpenGL::GL)

if (UNIX AND NOT APPLE)
    target_link_libraries(simulador_sjf PRIVATE pthread dl)
endif()

# Ocultar la consola de comandos en Windows
if (WIN32)
    target_link_libraries(simulador_sjf PRIVATE -mwindows)
    target_link_options(simulador_sjf PRIVATE -static-libgcc -static-libstdc++ -static -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive)
endif()