#pragma once
#include <string>

// ---------------------------------------------------------------------------
// Estados posibles de un proceso dentro del simulador.
// Corresponden al ciclo de vida clásico de un proceso en un SO
// multitarea: Nuevo -> Listo -> Ejecutando -> (Bloqueado) -> Finalizado
// ---------------------------------------------------------------------------
enum class ProcessState {
    NUEVO,
    ESPERANDO_MEMORIA, // Llegó, pero no hay memoria libre suficiente para admitirlo
    LISTO,
    EJECUTANDO,
    BLOQUEADO,
    FINALIZADO
};

const char* stateToString(ProcessState s);

// ---------------------------------------------------------------------------
// Representa un proceso simulado. Los tiempos se manejan en "ticks" de
// simulación (unidad de tiempo discreta y arbitraria).
// ---------------------------------------------------------------------------
struct Process {
    int id = -1;
    std::string name;

    // Parámetros de entrada
    int arrivalTime = 0;   // Tiempo de llegada
    int burstTime = 0;     // Ráfaga total de CPU requerida

    // Simulación opcional de un dispositivo de E/S: el proceso se bloquea
    // una vez cuando le queda "ioTriggerAt" de ráfaga restante.
    bool hasIO = false;
    int ioTriggerAt = 0;   // Se dispara la E/S cuando remainingTime llega a este valor
    int ioDuration = 0;    // Duración del bloqueo en ticks
    int ioRemaining = 0;   // Ticks de E/S restantes (en ejecución)
    bool ioTriggered = false;

    // Recurso simulado: memoria RAM requerida. Se compara contra la memoria
    // libre del sistema para decidir la admisión (ver Simulation::step).
    int memoryRequired = 0;
    bool everWaitedForMemory = false; // true si en algún tick estuvo ESPERANDO_MEMORIA

    // Estado dinámico
    ProcessState state = ProcessState::NUEVO;
    int remainingTime = 0;

    // Métricas
    int startTime = -1;    // Primer tick en que obtuvo la CPU (para tiempo de respuesta)
    int finishTime = -1;   // Tick en que finalizó

    void resetRuntimeState();

    // Tiempo de espera = finalización - llegada - ráfaga (sin contar E/S como espera)
    int waitingTime() const;
    // Tiempo de respuesta = primer inicio - llegada
    int responseTime() const;
    // Tiempo de retorno (turnaround) = finalización - llegada
    int turnaroundTime() const;
};
