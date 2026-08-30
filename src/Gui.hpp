#pragma once
#include "Simulation.hpp"
#include <string>

// ---------------------------------------------------------------------------
// Encapsula toda la interfaz Dear ImGui. Se le pasa una referencia a la
// simulación y en cada frame se llama a render().
// ---------------------------------------------------------------------------
class Gui {
public:
    explicit Gui(Simulation& sim);

    void render(); // Llamar una vez por frame, entre NewFrame() y Render()

private:
    Simulation& sim_;

    // Estado del formulario "agregar proceso"
    char newName_[32] = "P1";
    int newArrival_ = 0;
    int newBurst_ = 5;
    bool newHasIO_ = false;
    int newIoDuration_ = 2;
    int newMemory_ = 64;

    // Estado del generador aleatorio
    int randCount_ = 5;
    int randMaxArrival_ = 10;
    int randMinBurst_ = 2;
    int randMaxBurst_ = 10;
    bool randAllowIO_ = true;

    std::string exportStatus_;

    void drawSystemInfoPanel();
    void drawControlPanel();
    void drawGanttPanel();
    void drawReportPanel();
    void drawMetricsPanel();
    void drawInterpretationPanel();
};
