#include "Simulation.hpp"
#include "Scheduler.hpp"
#include <algorithm>
#include <random>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>

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

    // Si se agrega un proceso despues de haber corrido ticks (con play,
    // paso a paso, o generando aleatorios sobre una corrida ya iniciada),
    // el historial de snapshots quedaria con menos procesos de los que hay
    // ahora, y el Gantt se saldria de rango al leerlo. Por eso reiniciamos
    // la corrida (no la lista de procesos) cada vez que se agrega uno.
    reset();
}

void Simulation::generateRandomProcesses(int count, int maxArrival, int minBurst,
                                          int maxBurst, bool allowIO) {
    if (minBurst > maxBurst) std::swap(minBurst, maxBurst);
    if (minBurst < 1) minBurst = 1;
    if (maxBurst < minBurst) maxBurst = minBurst;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> arrivalDist(0, std::max(0, maxArrival));
    std::uniform_int_distribution<int> burstDist(minBurst, maxBurst);
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
    usedMemoryKB_ = 0;
    maxMemoryUsedKB_ = 0;
    isRunning = false;
}

void Simulation::reset() {
    for (auto& p : processes_) p.resetRuntimeState();
    history_.clear();
    currentTick_ = 0;
    cpuBusyTicks_ = 0;
    runningId_ = -1;
    accumulator_ = 0.0f;
    usedMemoryKB_ = 0;
    maxMemoryUsedKB_ = 0;
    isRunning = false;
}

bool Simulation::isFinished() const {
    if (processes_.empty()) return false;
    for (const auto& p : processes_)
        if (p.state != ProcessState::FINALIZADO) return false;
    return true;
}

void Simulation::updateMemoryAdmission() {
    // Memoria actualmente ocupada: todo proceso admitido y aún no finalizado
    // (LISTO, EJECUTANDO o BLOQUEADO) sigue reteniendo su memoria.
    int used = 0;
    for (const auto& p : processes_) {
        if (p.state == ProcessState::LISTO || p.state == ProcessState::EJECUTANDO ||
            p.state == ProcessState::BLOQUEADO) {
            used += p.memoryRequired;
        }
    }

    // Candidatos a admisión: ya llegaron pero siguen NUEVO o ESPERANDO_MEMORIA.
    // Se ordenan por llegada y luego por id para que la admisión sea FIFO y
    // determinística (evita que el orden del vector cambie el resultado).
    std::vector<Process*> candidates;
    for (auto& p : processes_) {
        if ((p.state == ProcessState::NUEVO || p.state == ProcessState::ESPERANDO_MEMORIA) &&
            p.arrivalTime <= currentTick_) {
            candidates.push_back(&p);
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Process* a, const Process* b) {
        if (a->arrivalTime != b->arrivalTime) return a->arrivalTime < b->arrivalTime;
        return a->id < b->id;
    });

    // Admisión "first-fit": si un proceso no cabe, se marca ESPERANDO_MEMORIA
    // pero se sigue evaluando a los siguientes (un proceso pequeño puede
    // colarse antes que uno grande que sigue esperando cupo). Es una
    // decisión de diseño documentada: prioriza el uso de la memoria libre
    // sobre la estricta justicia FIFO de admisión.
    for (Process* p : candidates) {
        if (used + p->memoryRequired <= config_.totalMemoryKB) {
            p->state = ProcessState::LISTO;
            used += p->memoryRequired;
        } else {
            p->state = ProcessState::ESPERANDO_MEMORIA;
            p->everWaitedForMemory = true;
        }
    }

    usedMemoryKB_ = used;
    if (usedMemoryKB_ > maxMemoryUsedKB_) maxMemoryUsedKB_ = usedMemoryKB_;
}

void Simulation::step() {
    if (isFinished()) { isRunning = false; return; }

    // 1) Llegadas + admisión por memoria: NUEVO/ESPERANDO_MEMORIA -> LISTO
    //    solo si hay memoria libre suficiente; si no, se queda esperando.
    updateMemoryAdmission();

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

std::string Simulation::buildInterpretationText() const {
    std::ostringstream out;

    if (processes_.empty()) {
        return "Agrega procesos y ejecuta la simulacion para ver la interpretacion.";
    }
    if (!isFinished()) {
        out << "La simulacion aun no termina (tick " << currentTick_
            << "). Los datos de abajo son parciales; corre hasta el final "
               "para una interpretacion completa.\n\n";
    }

    // Proceso con mayor tiempo de espera (entre los finalizados)
    const Process* peorEspera = nullptr;
    const Process* mejorEspera = nullptr;
    for (const auto& p : processes_) {
        if (p.state != ProcessState::FINALIZADO) continue;
        if (!peorEspera || p.waitingTime() > peorEspera->waitingTime()) peorEspera = &p;
        if (!mejorEspera || p.waitingTime() < mejorEspera->waitingTime()) mejorEspera = &p;
    }

    if (peorEspera && mejorEspera && peorEspera != mejorEspera) {
        out << "El proceso " << peorEspera->name << " (rafaga " << peorEspera->burstTime
            << ") fue el que mas tiempo de espera acumulo (" << peorEspera->waitingTime()
            << " ticks), mientras que " << mejorEspera->name << " (rafaga "
            << mejorEspera->burstTime << ") espero solo " << mejorEspera->waitingTime()
            << " ticks. ";
        if (peorEspera->burstTime > mejorEspera->burstTime) {
            out << "Esto es el comportamiento esperado de SJF/SRTF: al priorizar siempre "
                   "el proceso con menor tiempo restante, los procesos largos ceden la CPU "
                   "una y otra vez frente a procesos cortos que van llegando, lo que puede "
                   "derivar en inanicion (starvation) si llegan procesos cortos de forma "
                   "constante.\n\n";
        } else {
            out << "\n\n";
        }
    }

    // Utilizacion de CPU
    double util = cpuUtilizationPercent();
    out << "La utilizacion de CPU fue de " << std::fixed << std::setprecision(1) << util << "%. ";
    if (util >= 85.0) {
        out << "Es un valor alto: la CPU estuvo ocupada casi todo el tiempo, "
               "lo cual es eficiente.\n\n";
    } else if (util >= 50.0) {
        out << "Es un valor moderado: hubo tramos con la CPU ociosa, probablemente "
               "mientras se esperaban llegadas o se resolvia E/S.\n\n";
    } else {
        out << "Es un valor bajo: la CPU estuvo ociosa buena parte del tiempo, lo que "
               "sugiere pocos procesos listos compitiendo por ella (llegadas muy "
               "espaciadas o mucho tiempo bloqueado en E/S).\n\n";
    }

    // Memoria
    bool huboEsperaMemoria = false;
    for (const auto& p : processes_) if (p.everWaitedForMemory) huboEsperaMemoria = true;
    if (huboEsperaMemoria) {
        out << "En algun momento la memoria disponible (" << config_.totalMemoryKB
            << " KB) no alcanzo para admitir a todos los procesos que ya habian llegado, "
               "por lo que quedaron en estado 'Esperando memoria' hasta que otro proceso "
               "libero recursos al finalizar. El pico de uso de memoria fue de "
            << maxMemoryUsedKB_ << " KB.\n\n";
    } else {
        out << "La memoria configurada (" << config_.totalMemoryKB << " KB) fue suficiente "
               "en todo momento; el pico de uso fue de " << maxMemoryUsedKB_
            << " KB, sin que ningun proceso tuviera que esperar por este recurso.\n\n";
    }

    // E/S
    bool algunoConIO = false;
    for (const auto& p : processes_) if (p.hasIO) algunoConIO = true;
    if (algunoConIO) {
        out << "Los procesos con E/S liberaron la CPU mientras esperaban su dispositivo, "
               "lo cual le dio oportunidad a otros procesos listos de ejecutar durante "
               "ese bloqueo, contribuyendo a una mejor utilizacion de la CPU.\n";
    }

    return out.str();
}

bool Simulation::exportReport(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "==================================================\n";
    file << " INFORME DE SIMULACION - PLANIFICACION DE PROCESOS\n";
    file << "==================================================\n\n";

    file << "Algoritmo: SJF con desalojo (SRTF)\n";
    file << "Tipo de sistema: Monousuario / Multitarea\n";
    file << "Memoria total configurada: " << config_.totalMemoryKB << " KB\n";
    file << "Dispositivos de E/S: " << config_.ioDevices << "\n";
    file << "Duracion total de la simulacion: " << currentTick_ << " ticks\n\n";

    file << "--- Resultados por proceso ---\n";
    file << "PID\tNombre\tLlegada\tRafaga\tMemoria(KB)\tInicio\tFin\tEspera\tRespuesta\tRetorno\n";
    for (const auto& p : processes_) {
        file << p.id << "\t" << p.name << "\t" << p.arrivalTime << "\t" << p.burstTime << "\t"
             << p.memoryRequired << "\t"
             << (p.startTime >= 0 ? std::to_string(p.startTime) : "-") << "\t"
             << (p.finishTime >= 0 ? std::to_string(p.finishTime) : "-") << "\t"
             << (p.state == ProcessState::FINALIZADO ? std::to_string(p.waitingTime()) : "-") << "\t"
             << (p.startTime >= 0 ? std::to_string(p.responseTime()) : "-") << "\t"
             << (p.state == ProcessState::FINALIZADO ? std::to_string(p.turnaroundTime()) : "-")
             << "\n";
    }

    file << "\n--- Metricas globales ---\n";
    file << "Tiempo de espera promedio: " << averageWaitingTime() << " ticks\n";
    file << "Tiempo de respuesta promedio: " << averageResponseTime() << " ticks\n";
    file << "Utilizacion de CPU: " << cpuUtilizationPercent() << " %\n";
    file << "Memoria maxima utilizada: " << maxMemoryUsedKB_ << " KB de " << config_.totalMemoryKB << " KB\n";

    file << "\n--- Interpretacion ---\n";
    file << buildInterpretationText() << "\n";

    return true;
}

bool Simulation::exportReportHtml(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    // Convertimos el texto de interpretacion (separado por lineas en blanco)
    // en parrafos <p> normales, en vez de mostrarlo en una sola caja.
    std::string interpretacion = buildInterpretationText();
    std::ostringstream parrafos;
    {
        std::istringstream stream(interpretacion);
        std::string linea;
        std::string parrafo;
        while (std::getline(stream, linea)) {
            if (linea.empty()) {
                if (!parrafo.empty()) {
                    parrafos << "<p>" << parrafo << "</p>\n";
                    parrafo.clear();
                }
            } else {
                if (!parrafo.empty()) parrafo += " ";
                parrafo += linea;
            }
        }
        if (!parrafo.empty()) parrafos << "<p>" << parrafo << "</p>\n";
    }

    file << "<!DOCTYPE html>\n<html lang=\"es\">\n<head>\n<meta charset=\"UTF-8\">\n";
    file << "<title>Informe de Simulacion - Planificacion de Procesos</title>\n";
    file << "<style>\n"
            "  body { font-family: Arial, Helvetica, sans-serif; max-width: 900px; "
            "margin: 32px auto; color: #1c2530; line-height: 1.5; }\n"
            "  h1 { font-size: 22px; border-bottom: 3px solid #2563eb; padding-bottom: 8px; }\n"
            "  h2 { font-size: 17px; color: #2563eb; margin-top: 32px; }\n"
            "  .meta { color: #6b7280; margin-bottom: 24px; }\n"
            "  table { border-collapse: collapse; width: 100%; margin-top: 8px; font-size: 14px; }\n"
            "  th, td { border: 1px solid #e5e7eb; padding: 6px 10px; text-align: center; }\n"
            "  th { background: #2563eb; color: white; }\n"
            "  tr:nth-child(even) { background: #f7f8fa; }\n"
            "  .metricas { list-style: none; padding: 0; }\n"
            "  .metricas li { padding: 4px 0; }\n"
            "  .interpretacion p { background: #f7f8fa; padding: 10px 14px; border-left: 4px "
            "solid #2563eb; border-radius: 4px; margin: 10px 0; }\n"
            "  .tip { color: #6b7280; font-size: 12px; margin-top: 40px; border-top: 1px solid "
            "#e5e7eb; padding-top: 10px; }\n"
            "</style>\n</head>\n<body>\n";

    file << "<h1>Informe de Simulacion - Planificacion de Procesos</h1>\n";
    file << "<p class=\"meta\">Algoritmo: SJF con desalojo (SRTF) &nbsp;|&nbsp; "
            "Tipo de sistema: Monousuario / Multitarea &nbsp;|&nbsp; Memoria total: "
         << config_.totalMemoryKB << " KB &nbsp;|&nbsp; Dispositivos de E/S: "
         << config_.ioDevices << " &nbsp;|&nbsp; Duracion: " << currentTick_ << " ticks</p>\n";

    file << "<h2>Resultados por proceso</h2>\n<table>\n<tr>"
            "<th>PID</th><th>Nombre</th><th>Llegada</th><th>Rafaga</th><th>Memoria (KB)</th>"
            "<th>Inicio</th><th>Fin</th><th>Espera</th><th>Respuesta</th><th>Retorno</th></tr>\n";
    for (const auto& p : processes_) {
        file << "<tr><td>" << p.id << "</td><td>" << p.name << "</td><td>" << p.arrivalTime
             << "</td><td>" << p.burstTime << "</td><td>" << p.memoryRequired << "</td><td>"
             << (p.startTime >= 0 ? std::to_string(p.startTime) : "-") << "</td><td>"
             << (p.finishTime >= 0 ? std::to_string(p.finishTime) : "-") << "</td><td>"
             << (p.state == ProcessState::FINALIZADO ? std::to_string(p.waitingTime()) : "-")
             << "</td><td>" << (p.startTime >= 0 ? std::to_string(p.responseTime()) : "-")
             << "</td><td>"
             << (p.state == ProcessState::FINALIZADO ? std::to_string(p.turnaroundTime()) : "-")
             << "</td></tr>\n";
    }
    file << "</table>\n";

    file << "<h2>Metricas globales</h2>\n<ul class=\"metricas\">\n";
    file << "<li><b>Tiempo de espera promedio:</b> " << averageWaitingTime() << " ticks</li>\n";
    file << "<li><b>Tiempo de respuesta promedio:</b> " << averageResponseTime() << " ticks</li>\n";
    file << "<li><b>Utilizacion de CPU:</b> " << cpuUtilizationPercent() << " %</li>\n";
    file << "<li><b>Memoria maxima utilizada:</b> " << maxMemoryUsedKB_ << " KB de "
         << config_.totalMemoryKB << " KB</li>\n</ul>\n";

    file << "<h2>Interpretacion</h2>\n<div class=\"interpretacion\">\n" << parrafos.str()
         << "</div>\n";

    file << "<p class=\"tip\">Sugerencia: para entregar este informe como PDF, abre este "
            "archivo en tu navegador (doble clic) y usa Ctrl+P &gt; Guardar como PDF.</p>\n";

    file << "</body>\n</html>\n";
    return true;
}
