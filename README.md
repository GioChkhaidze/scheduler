# Scheduler

A C++20 interactive scheduler for the distributed inference scheduling challenge. The repository now keeps the
best judged scheduler, the protocol/state model, the exact local simulator, and focused correctness tests.

## Build and test

```powershell
cmake --workflow --preset dev
```

This configures a Debug build, compiles both executables, and runs every test with failure output enabled.

## Project layout

```text
include/scheduler/
  model/            Public configuration, protocol, timing, and world-state interfaces
  scheduling/       Public scheduling policy interfaces
  simulation/       Public simulator and observability interfaces
  Scheduler.hpp     Convenience header for all scheduling policies
src/app/            Interactive scheduler and simulator entry points
src/model/          Configuration, timing curves, protocol parsing, and world-state transitions
src/scheduling/     Scheduling implementations, analysis, and the private scheduling core
src/simulation/     Discrete-event simulation and runtime observability
tests/              Interface-level correctness and integration tests
```

Public headers use explicit domain-qualified includes, for example
`#include <scheduler/model/WorldState.hpp>`. `src/scheduling/SchedulerCore.hpp` intentionally remains in `src`:
it contains private template implementation shared only by scheduling policies and is not part of the caller-facing API.

## Run the interactive scheduler

```powershell
Get-Content -Raw example.txt | ./build/Debug/scheduler.exe
```

`submission.cpp` is the final self-contained Codeforces submission.

## Run the simulator

A simulator scenario contains the normal system configuration and task-time table, followed by a request count and
one `arrival_time input_length output_length` row per request.

```powershell
Get-Content -Raw tests/data/example1_scenario.txt | ./build/Debug/scheduler_simulator.exe
Get-Content -Raw tests/data/example1_scenario.txt | ./build/Debug/scheduler_simulator.exe --trace --observe
```

The simulator reports completion, throughput, TDR, TPOT, distance, and the exact score. `--trace` prints event-frame
summaries; `--observe` adds resource utilization, queue pressure, and decode batch distributions.

## Documentation

- [Problem statement](docs/statement.pdf)
- [From problem statement to correct system](docs/problem-solving-method.md)
