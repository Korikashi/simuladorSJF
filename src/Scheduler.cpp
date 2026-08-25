#include "Scheduler.hpp"
#include <queue>

namespace {

// Comparador para el min-heap: menor tiempo restante tiene mayor prioridad.
// En caso de empate, desempata por tiempo de llegada y luego por id,
// para que el resultado sea determinístico.
struct RemainingTimeGreater {
    bool operator()(const Process* a, const Process* b) const {
        if (a->remainingTime != b->remainingTime)
            return a->remainingTime > b->remainingTime; // invertido: priority_queue es max-heap
        if (a->arrivalTime != b->arrivalTime)
            return a->arrivalTime > b->arrivalTime;
        return a->id > b->id;
    }
};

} // namespace

Process* SJFScheduler::selectNext(std::vector<Process*>& candidates) {
    if (candidates.empty()) return nullptr;

    std::priority_queue<Process*, std::vector<Process*>, RemainingTimeGreater> pq(
        candidates.begin(), candidates.end());

    return pq.top();
}
