# Simulador de Planificación de Procesos — SJF (SRTF)

Simulación de un planificador de procesos para un sistema operativo
**Monousuario / Multitarea**, con interfaz gráfica en **Dear ImGui**.

## Vídeo de Simulación  
[![▶️ Ver video en YouTube](https://img.shields.io/badge/_Ver_video_en_YouTube-FF0000?style=for-the-badge\&logo=youtube\&logoColor=white)](https://youtu.be/ew-jsleNsX4)

## 1. Descripción del sistema simulado

| Parámetro                  | Valor                                                       |
|-----------------------------|--------------------------------------------------------------|
| Tipo de SO                 | Monousuario / Multitarea                                     |
| Algoritmo de planificación  | **SJF con desalojo (SRTF – Shortest Remaining Time First)** |
| CPU                         | 1 (compartida entre todos los procesos)                     |
| Memoria RAM                 | Capacidad total configurable + memoria requerida por proceso|
| Dispositivo de E/S          | 1 (opcional por proceso, bloquea al llegar a la mitad de su ráfaga restante) |
| Tiempos                     | Fijos (formulario) o aleatorios (generador)                 |

## 2. Estructura del código

```
scheduler_sim/
├── CMakeLists.txt          # Build system, descarga GLFW + Dear ImGui
├── src/
│   ├── Process.hpp/.cpp    # Modelo de datos: Process, ProcessState, métricas por proceso
│   ├── Scheduler.hpp/.cpp  # Algoritmo SJF/SRTF (std::priority_queue)
│   ├── Simulation.hpp/.cpp # Motor de simulación: ticks, colas, E/S, métricas globales
│   ├── Gui.hpp/.cpp        # Toda la interfaz Dear ImGui (paneles)
│   └── main.cpp            # Inicialización de GLFW/OpenGL3 y bucle principal
```

## 3. Algoritmo (SJF/SRTF) — lógica clave

En cada **tick** de simulación (`Simulation::step()`):

1. Los procesos cuyo `arrivalTime` llegó pasan de `NUEVO` a `LISTO`.
2. Los procesos `BLOQUEADO` (en E/S) decrementan su tiempo de E/S restante;
   al llegar a 0 vuelven a `LISTO`.
3. Se arman los candidatos (`LISTO` + el que esté `EJECUTANDO`) y se eligen
   con un **`std::priority_queue`** ordenado por menor tiempo restante de
   CPU (`Scheduler.cpp`). Si el proceso elegido no es el que estaba
   corriendo, éste es **desalojado** (vuelve a `LISTO`) — esto es lo que
   hace que el algoritmo sea SRTF y no SJF puro no apropiativo.
4. El proceso seleccionado ejecuta 1 tick de CPU. Si llega a su punto de
   E/S se bloquea; si termina su ráfaga pasa a `FINALIZADO`.
5. Se guarda un snapshot del estado de todos los procesos para dibujar el
   diagrama de Gantt.

## 4. Métricas (`Simulation::average*`, `cpuUtilizationPercent`)

- **Tiempo de espera promedio** = promedio de `(fin − llegada − ráfaga)` de
  los procesos finalizados.
- **Tiempo de respuesta promedio** = promedio de `(primer inicio − llegada)`.
- **Utilización de CPU (%)** = `(ticks de CPU ocupada / ticks totales) × 100`.

## 5. Interfaz gráfica

- **Panel de Control**: Play / Pausa / Paso a paso / control de velocidad,
  formulario para agregar procesos manualmente y generador de procesos
  aleatorios.
- **Diagrama de Gantt**: una fila por proceso, una celda de color por tick
  (gris=Nuevo, amarillo=Listo, verde=Ejecutando, rojo=Bloqueado,
  azul=Finalizado), con scroll horizontal.
- **Métricas de Rendimiento**: espera promedio, respuesta promedio y
  utilización de CPU, actualizadas en vivo.
- **Reporte Final**: tabla con llegada, ráfaga, memoria, inicio, fin,
  espera y respuesta por proceso.

## 6. Compilar y ejecutar

Requisitos: CMake ≥ 3.16, un compilador C++17, y en Linux las
dependencias de desarrollo de X11/OpenGL que usa GLFW.

### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install -y build-essential cmake git \
    libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev

cd scheduler_sim
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/simulador_sjf
```

### Windows (Visual Studio) / macOS
```bash
cd scheduler_sim
cmake -S . -B build
cmake --build build --config Release
```
Luego ejecuta el binario generado en `build/` (o `build/Release/` en
Windows).

La primera configuración con CMake descargará automáticamente **GLFW** y
**Dear ImGui** desde GitHub (vía `FetchContent`), así que se necesita
conexión a internet la primera vez.


