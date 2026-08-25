#include "Process.hpp"

const char* stateToString(ProcessState s) {
    switch (s) {
        case ProcessState::NUEVO:      return "Nuevo";
        case ProcessState::LISTO:      return "Listo";
        case ProcessState::EJECUTANDO: return "Ejecutando";
        case ProcessState::BLOQUEADO:  return "Bloqueado";
        case ProcessState::FINALIZADO: return "Finalizado";
    }
    return "?";
}

void Process::resetRuntimeState() {
    state = ProcessState::NUEVO;
    remainingTime = burstTime;
    ioRemaining = 0;
    ioTriggered = false;
    startTime = -1;
    finishTime = -1;
}

int Process::waitingTime() const {
    if (finishTime < 0) return 0;
    int w = finishTime - arrivalTime - burstTime;
    return w < 0 ? 0 : w;
}

int Process::responseTime() const {
    if (startTime < 0) return 0;
    return startTime - arrivalTime;
}

int Process::turnaroundTime() const {
    if (finishTime < 0) return 0;
    return finishTime - arrivalTime;
}
