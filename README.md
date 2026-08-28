# Edge–Cloud Collaborative Scheduler

My C++20 scheduler implementation for the ICPC 2026 Online Challenge 1 and submission for
[A. Edge–Cloud Collaborative Scheduling](https://codeforces.com/contest/2251/problem/A), its interactive
optimization problem.

The scheduler coordinates edge and cloud resources to process inference requests while balancing output throughput
against request waiting time. This repository contains the modular scheduler implementation, protocol and state
model, local simulator, correctness tests, and the final self-contained `submission.cpp`.

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
