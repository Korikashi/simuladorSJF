// Simulador de planificacion de procesos - SJF con desalojo (SRTF)
// SO simulado: Monousuario / Multitarea | 1 CPU, RAM, 1 dispositivo E/S
//
// GUI: Dear ImGui + GLFW + OpenGL3

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cstdio>

#include "Simulation.hpp"
#include "Gui.hpp"

static void glfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "Error GLFW %d: %s\n", error, description);
}

int main() {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        return 1;
    }

    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 800,
        "Simulador de Planificacion de Procesos - SJF (SRTF)", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io; // reservado por si luego se habilitan mas ImGuiIO flags

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    Simulation simulation;
    // Carga algunos procesos de ejemplo al iniciar
    simulation.addProcess("P1", 0, 8, true, 2, 64);
    simulation.addProcess("P2", 1, 4, false, 0, 32);
    simulation.addProcess("P3", 2, 9, false, 0, 128);
    simulation.addProcess("P4", 3, 5, true, 3, 64);

    Gui gui(simulation);

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        simulation.update(dt);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        gui.render();

        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
