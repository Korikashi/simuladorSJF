#include "Gui.hpp"
#include "imgui.h"
#include <cstdio>

namespace {

ImVec4 colorForState(ProcessState s) {
    switch (s) {
        case ProcessState::NUEVO:      return ImVec4(0.55f, 0.55f, 0.55f, 1.0f); // gris
        case ProcessState::LISTO:      return ImVec4(0.90f, 0.80f, 0.20f, 1.0f); // amarillo
        case ProcessState::EJECUTANDO: return ImVec4(0.20f, 0.75f, 0.30f, 1.0f); // verde
        case ProcessState::BLOQUEADO:  return ImVec4(0.85f, 0.30f, 0.20f, 1.0f); // rojo/naranja
        case ProcessState::FINALIZADO: return ImVec4(0.25f, 0.45f, 0.85f, 1.0f); // azul
    }
    return ImVec4(1, 1, 1, 1);
}

} // namespace

Gui::Gui(Simulation& sim) : sim_(sim) {}

void Gui::render() {
    drawSystemInfoPanel();
    drawControlPanel();
    drawGanttPanel();
    drawMetricsPanel();
    drawReportPanel();
}

void Gui::drawSystemInfoPanel() {
    ImGui::Begin("Configuracion del Sistema Operativo (simulado)");
    ImGui::Text("Tipo de SO: Monousuario / Multitarea");
    ImGui::Text("Algoritmo de planificacion: SJF con desalojo (SRTF)");
    ImGui::Separator();
    ImGui::Text("Recursos simulados:");
    ImGui::BulletText("1 CPU");
    ImGui::SliderInt("Memoria RAM total (KB)", &sim_.config().totalMemoryKB, 128, 8192);
    ImGui::BulletText("Dispositivos de E/S: %d", sim_.config().ioDevices);
    ImGui::End();
}

void Gui::drawControlPanel() {
    ImGui::Begin("Panel de Control");

    ImGui::SeparatorText("Ejecucion");
    if (sim_.isRunning) {
        if (ImGui::Button("Pausa", ImVec2(90, 0))) sim_.isRunning = false;
    } else {
        if (ImGui::Button("Play", ImVec2(90, 0))) sim_.isRunning = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Paso a paso", ImVec2(110, 0))) {
        sim_.isRunning = false;
        sim_.step();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reiniciar", ImVec2(90, 0))) {
        sim_.reset();
    }
    ImGui::SliderFloat("Velocidad (ticks/seg)", &sim_.ticksPerSecond, 0.5f, 10.0f, "%.1f");

    ImGui::Text("Tick actual: %d", sim_.getCurrentTick());
    ImGui::Text("Estado: %s", sim_.isFinished() ? "Simulacion finalizada"
                                                 : (sim_.isRunning ? "Ejecutando" : "Pausado"));

    ImGui::SeparatorText("Agregar proceso manualmente");
    ImGui::InputText("Nombre", newName_, IM_ARRAYSIZE(newName_));
    ImGui::InputInt("Tiempo de llegada", &newArrival_);
    ImGui::InputInt("Rafaga de CPU", &newBurst_);
    ImGui::InputInt("Memoria requerida (KB)", &newMemory_);
    ImGui::Checkbox("Requiere E/S", &newHasIO_);
    if (newHasIO_) {
        ImGui::InputInt("Duracion E/S (ticks)", &newIoDuration_);
    }
    if (ImGui::Button("Agregar proceso")) {
        if (newArrival_ < 0) newArrival_ = 0;
        if (newBurst_ < 1) newBurst_ = 1;
        sim_.addProcess(newName_, newArrival_, newBurst_, newHasIO_, newIoDuration_, newMemory_);
        std::snprintf(newName_, sizeof(newName_), "P%d", (int)sim_.getProcesses().size() + 1);
    }

    ImGui::SeparatorText("Generar procesos aleatorios");
    ImGui::InputInt("Cantidad", &randCount_);
    ImGui::InputInt("Llegada maxima", &randMaxArrival_);
    ImGui::InputInt("Rafaga minima", &randMinBurst_);
    ImGui::InputInt("Rafaga maxima", &randMaxBurst_);
    ImGui::Checkbox("Permitir E/S aleatoria", &randAllowIO_);
    if (ImGui::Button("Generar aleatorios")) {
        if (randCount_ > 0) {
            sim_.generateRandomProcesses(randCount_, randMaxArrival_, randMinBurst_,
                                          randMaxBurst_, randAllowIO_);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Limpiar todos")) {
        sim_.clearProcesses();
    }

    ImGui::End();
}

void Gui::drawGanttPanel() {
    ImGui::Begin("Diagrama de Gantt / Estado en tiempo real");

    ImGui::TextColored(colorForState(ProcessState::LISTO), "Listo");
    ImGui::SameLine();
    ImGui::TextColored(colorForState(ProcessState::EJECUTANDO), "Ejecutando");
    ImGui::SameLine();
    ImGui::TextColored(colorForState(ProcessState::BLOQUEADO), "Bloqueado");
    ImGui::SameLine();
    ImGui::TextColored(colorForState(ProcessState::FINALIZADO), "Finalizado");
    ImGui::Separator();

    const auto& procs = sim_.getProcesses();
    const auto& history = sim_.getHistory();

    if (procs.empty()) {
        ImGui::TextDisabled("Agrega o genera procesos para ver el diagrama.");
        ImGui::End();
        return;
    }

    const float cellW = 14.0f;
    const float cellH = 22.0f;
    const float labelW = 70.0f;

    ImGui::BeginChild("gantt_scroll", ImVec2(0, (float)procs.size() * (cellH + 4) + 20),
                       true, ImGuiWindowFlags_HorizontalScrollbar);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    for (size_t row = 0; row < procs.size(); ++row) {
        float y = origin.y + row * (cellH + 4);
        char label[48];
        std::snprintf(label, sizeof(label), "%s (id %d)", procs[row].name.c_str(), procs[row].id);
        draw->AddText(ImVec2(origin.x, y + 4), IM_COL32(230, 230, 230, 255), label);

        for (size_t t = 0; t < history.size(); ++t) {
            ProcessState st = history[t].states[row];
            ImVec4 c = colorForState(st);
            ImVec2 p0(origin.x + labelW + t * cellW, y);
            ImVec2 p1(p0.x + cellW - 1, y + cellH - 1);
            draw->AddRectFilled(p0, p1, IM_COL32((int)(c.x * 255), (int)(c.y * 255), (int)(c.z * 255), 255));
        }
    }

    // Reservar espacio para que el scroll horizontal funcione correctamente
    ImGui::Dummy(ImVec2(labelW + history.size() * cellW, procs.size() * (cellH + 4)));
    ImGui::EndChild();

    ImGui::End();
}

void Gui::drawMetricsPanel() {
    ImGui::Begin("Metricas de Rendimiento");

    ImGui::SeparatorText("Resultados globales");
    ImGui::Text("Tiempo de espera promedio: %.2f ticks", sim_.averageWaitingTime());
    ImGui::Text("Tiempo de respuesta promedio: %.2f ticks", sim_.averageResponseTime());
    ImGui::Text("Utilizacion de CPU: %.1f %%", sim_.cpuUtilizationPercent());

    ImGui::ProgressBar((float)(sim_.cpuUtilizationPercent() / 100.0), ImVec2(-1, 0));

    ImGui::End();
}

void Gui::drawReportPanel() {
    ImGui::Begin("Reporte Final");

    const auto& procs = sim_.getProcesses();
    if (ImGui::BeginTable("reporte", 9,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Nombre");
        ImGui::TableSetupColumn("Llegada");
        ImGui::TableSetupColumn("Rafaga");
        ImGui::TableSetupColumn("Memoria (KB)");
        ImGui::TableSetupColumn("Inicio");
        ImGui::TableSetupColumn("Fin");
        ImGui::TableSetupColumn("T. Espera");
        ImGui::TableSetupColumn("T. Respuesta");
        ImGui::TableHeadersRow();

        for (const auto& p : procs) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", p.id);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", p.name.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%d", p.arrivalTime);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%d", p.burstTime);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%d", p.memoryRequired);
            ImGui::TableSetColumnIndex(5); ImGui::Text("%s", p.startTime >= 0 ? std::to_string(p.startTime).c_str() : "-");
            ImGui::TableSetColumnIndex(6); ImGui::Text("%s", p.finishTime >= 0 ? std::to_string(p.finishTime).c_str() : "-");
            ImGui::TableSetColumnIndex(7); ImGui::Text("%s", p.state == ProcessState::FINALIZADO ? std::to_string(p.waitingTime()).c_str() : "-");
            ImGui::TableSetColumnIndex(8); ImGui::Text("%s", p.startTime >= 0 ? std::to_string(p.responseTime()).c_str() : "-");
        }
        ImGui::EndTable();
    }

    if (sim_.isFinished() && !procs.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Simulacion completa.");
        ImGui::Text("Espera promedio: %.2f | Respuesta promedio: %.2f | CPU: %.1f%%",
                    sim_.averageWaitingTime(), sim_.averageResponseTime(), sim_.cpuUtilizationPercent());
    }

    ImGui::End();
}
