#include "Simulation.hpp"
#include "Scheduler.hpp"
#include <algorithm>
#include <random>
#include <cmath>

Simulation::Simulation() {
    reset();
}

Process* Simulation::findById(int id) {
    for (auto& p : processes_) if (p.id == id) return &p;
    return nullptr;
}

void Simulation::addProcess(const std::string& name, int arrival, int burst,
                             bool hasIO, int ioDuration, int memoryKB) {
    Process p;
    p.id = nextId_++;
    p.name = name;
    p.arrivalTime = std::max(0, arrival);
    p.burstTime = std::max(1, burst);
    p.hasIO = hasIO && p.burstTime > 1;
    p.ioDuration = std::max(1, ioDuration);
    // La E/S se dispara aproximadamente a la mitad de la ráfaga restante.
    p.ioTriggerAt = p.hasIO ? std::max(1, p.burstTime / 2) : 0;
    p.memoryRequired = std::max(0, memoryKB);
    p.resetRuntimeState();
    processes_.push_back(p);
}

void Simulation::generateRandomProcesses(int count, int maxArrival, int minBurst,
                                          int maxBurst, bool allowIO) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> arrivalDist(0, std::max(0, maxArrival));
    std::uniform_int_distribution<int> burstDist(std::max(1, minBurst), std::max(minBurst, maxBurst));
    std::uniform_int_distribution<int> ioDist(0, 1);
    std::uniform_int_distribution<int> ioDurDist(1, 4);
    std::uniform_int_distribution<int> memDist(32, 256);

    for (int i = 0; i < count; ++i) {
        std::string name = "P" + std::to_string(nextId_);
        bool hasIO = allowIO && ioDist(gen) == 1;
        addProcess(name, arrivalDist(gen), burstDist(gen), hasIO, ioDurDist(gen), memDist(gen));
    }
}

void Simulation::clearProcesses() {
    processes_.clear();
    history_.clear();
    currentTick_ = 0;
    cpuBusyTicks_ = 0;
    runningId_ = -1;
    accumulator_ = 0.0f;
    nextId_ = 1;
    isRunning = false;
}

void Simulation::reset() {
    for (auto& p : processes_) p.resetRuntimeState();
    history_.clear();
    currentTick_ = 0;
    cpuBusyTicks_ = 0;
    runningId_ = -1;
    accumulator_ = 0.0f;
    isRunning = false;
}

bool Simulation::isFinished() const {
    if (processes_.empty()) return false;
    for (const auto& p : processes_)
        if (p.state != ProcessState::FINALIZADO) return false;
    return true;
}

void Simulation::step() {
    if (isFinished()) { isRunning = false; return; }

    // 1) Llegadas: NUEVO -> LISTO cuando arrivalTime == tick actual
    for (auto& p : processes_) {
        if (p.state == ProcessState::NUEVO && p.arrivalTime <= currentTick_) {
            p.state = ProcessState::LISTO;
        }
    }

    // 2) Avance de E/S: BLOQUEADO -> LISTO cuando termina el bloqueo
    for (auto& p : processes_) {
        if (p.state == ProcessState::BLOQUEADO) {
            p.ioRemaining--;
            if (p.ioRemaining <= 0) {
                p.state = ProcessState::LISTO;
            }
        }
    }

    // 3) Selección SJF/SRTF: candidatos = LISTO + el que está EJECUTANDO
    std::vector<Process*> candidates;
    for (auto& p : processes_) {
        if (p.state == ProcessState::LISTO || p.state == ProcessState::EJECUTANDO) {
            candidates.push_back(&p);
        }
    }
    Process* next = SJFScheduler::selectNext(candidates);

    if (runningId_ != -1 && (next == nullptr || next->id != runningId_)) {
        // Desalojo: el proceso que estaba corriendo vuelve a LISTO
        Process* prevRunning = findById(runningId_);
        if (prevRunning && prevRunning->state == ProcessState::EJECUTANDO) {
            prevRunning->state = ProcessState::LISTO;
        }
        runningId_ = -1;
    }

    if (next != nullptr) {
        if (next->startTime < 0) next->startTime = currentTick_;
        next->state = ProcessState::EJECUTANDO;
        runningId_ = next->id;
    }

    // 4) Ejecutar 1 tick de CPU sobre el proceso en ejecución
    if (runningId_ != -1) {
        Process* running = findById(runningId_);
        cpuBusyTicks_++;
        running->remainingTime--;

        if (running->hasIO && !running->ioTriggered &&
            running->remainingTime <= running->ioTriggerAt && running->remainingTime > 0) {
            // Se dispara la E/S: el proceso libera la CPU y se bloquea
            running->ioTriggered = true;
            running->ioRemaining = running->ioDuration;
            running->state = ProcessState::BLOQUEADO;
            runningId_ = -1;
        } else if (running->remainingTime <= 0) {
            running->remainingTime = 0;
            running->state = ProcessState::FINALIZADO;
            running->finishTime = currentTick_ + 1;
            runningId_ = -1;
        }
    }

    // 5) Registrar snapshot del tick para el diagrama de Gantt
    TickSnapshot snap;
    snap.states.reserve(processes_.size());
    for (const auto& p : processes_) snap.states.push_back(p.state);
    snap.runningProcessId = runningId_;
    history_.push_back(snap);

    currentTick_++;
}

void Simulation::update(float dtSeconds) {
    if (!isRunning) return;
    if (isFinished()) { isRunning = false; return; }

    accumulator_ += dtSeconds;
    float ticksInterval = 1.0f / std::max(0.1f, ticksPerSecond);
    while (accumulator_ >= ticksInterval) {
        accumulator_ -= ticksInterval;
        step();
        if (isFinished()) { isRunning = false; break; }
    }
}

double Simulation::averageWaitingTime() const {
    int n = 0;
    long long sum = 0;
    for (const auto& p : processes_) {
        if (p.state == ProcessState::FINALIZADO) { sum += p.waitingTime(); n++; }
    }
    return n == 0 ? 0.0 : static_cast<double>(sum) / n;
}

double Simulation::averageResponseTime() const {
    int n = 0;
    long long sum = 0;
    for (const auto& p : processes_) {
        if (p.startTime >= 0) { sum += p.responseTime(); n++; }
    }
    return n == 0 ? 0.0 : static_cast<double>(sum) / n;
}

double Simulation::cpuUtilizationPercent() const {
    if (currentTick_ == 0) return 0.0;
    return (static_cast<double>(cpuBusyTicks_) / static_cast<double>(currentTick_)) * 100.0;
}
