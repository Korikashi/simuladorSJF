#pragma once
#include <vector>
#include "Process.hpp"

// ---------------------------------------------------------------------------
// Planificador SJF (Shortest Job First) con desalojo -> equivalente a
// SRTF (Shortest Remaining Time First).
//
// En cada tick de simulación se evalúan todos los procesos en estado LISTO
// (más el que está EJECUTANDO, si lo hay) y se selecciona el de menor
// tiempo restante de CPU. Si un proceso recién llegado tiene una ráfaga
// restante menor a la del proceso en ejecución, éste es desalojado
// (vuelve a LISTO) y el nuevo pasa a EJECUTANDO.
//
// Internamente se usa std::priority_queue (min-heap por tiempo restante)
// para decidir el próximo proceso a ejecutar, tal como pide el enunciado.
// ---------------------------------------------------------------------------
class SJFScheduler {
public:
    // Recibe los procesos candidatos (LISTO o EJECUTANDO) y retorna un
    // puntero al proceso que debe ocupar la CPU en el próximo tick.
    // Retorna nullptr si no hay candidatos.
    static Process* selectNext(std::vector<Process*>& candidates);
};
