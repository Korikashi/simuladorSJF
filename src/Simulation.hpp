#pragma once
#include <vector>
#include <string>
#include "Process.hpp"

// ---------------------------------------------------------------------------
// Configuración simulada del "sistema operativo".
// Este simulador modela un SO Monousuario / Multitarea:
//   - Monousuario: un solo usuario controla la simulación desde la GUI.
//   - Multitarea: múltiples procesos conviven en memoria y se alternan
//     el uso de 1 CPU física, siguiendo el algoritmo SJF/SRTF.
// Recursos simulados: 1 CPU, memoria RAM con capacidad fija y 1 dispositivo
// de E/S compartido.
// ---------------------------------------------------------------------------
struct SystemConfig {
    int totalMemoryKB = 1024;   // Memoria RAM total simulada (capacidad fija)
    int ioDevices = 1;          // Número de dispositivos de E/S simulados (fijo en 1)
};

// Snapshot de un tick, usado para dibujar el diagrama de Gantt.
struct TickSnapshot {
    std::vector<ProcessState> states; // Estado de cada proceso en ese tick, mismo orden que 'processes'
    int runningProcessId = -1;
};

class Simulation {
public:
    Simulation();

    // --- Configuración de procesos -----------------------------------
    void addProcess(const std::string& name, int arrival, int burst,
                     bool hasIO, int ioDuration, int memoryKB);
    void generateRandomProcesses(int count, int maxArrival, int minBurst,
                                  int maxBurst, bool allowIO);
    void clearProcesses();
    void reset(); // Reinicia la simulación manteniendo los procesos cargados

    // --- Control de ejecución ------------------------------------------
    void step();                 // Avanza un (1) tick de simulación
    void update(float dtSeconds); // Auto-avance según 'ticksPerSecond' si isRunning
    bool isFinished() const;

    // --- Estado expuesto a la GUI ---------------------------------------
    std::vector<Process>& getProcesses() { return processes_; }
    const std::vector<Process>& getProcesses() const { return processes_; }
    const std::vector<TickSnapshot>& getHistory() const { return history_; }
    int getCurrentTick() const { return currentTick_; }
    int getRunningProcessId() const { return runningId_; }
    SystemConfig& config() { return config_; }

    bool isRunning = false;      // Play / Pause
    float ticksPerSecond = 2.0f; // Controla la velocidad de auto-avance

    // --- Métricas de rendimiento ----------------------------------------
    double averageWaitingTime() const;
    double averageResponseTime() const;
    double cpuUtilizationPercent() const;
    int getUsedMemoryKB() const { return usedMemoryKB_; }
    int getMaxMemoryUsedKB() const { return maxMemoryUsedKB_; }

    // --- Interpretación e informes --------------------------------------
    // Genera un texto en lenguaje natural interpretando los resultados
    // actuales de la simulación (se usa tanto en la GUI como en el export).
    std::string buildInterpretationText() const;
    // Escribe un informe .txt con configuración, tabla de resultados,
    // métricas globales e interpretación. Devuelve true si pudo escribir.
    bool exportReport(const std::string& path) const;
    // Version HTML del mismo informe: tabla real con estilos, mas facil de
    // leer y de convertir a PDF (abrir en el navegador -> Ctrl+P -> Guardar
    // como PDF). Devuelve true si pudo escribir.
    bool exportReportHtml(const std::string& path) const;

private:
    std::vector<Process> processes_;
    std::vector<TickSnapshot> history_;

    SystemConfig config_;
    int currentTick_ = 0;
    int cpuBusyTicks_ = 0;
    int runningId_ = -1; // id del proceso actualmente en CPU, -1 si ninguno
    float accumulator_ = 0.0f;
    int nextId_ = 1;
    int usedMemoryKB_ = 0;    // Memoria actualmente ocupada por procesos admitidos
    int maxMemoryUsedKB_ = 0; // Pico histórico de memoria ocupada

    Process* findById(int id);
    void updateMemoryAdmission(); // NUEVO/ESPERANDO_MEMORIA -> LISTO según memoria libre
};
