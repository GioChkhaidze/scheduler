# Scheduler

## Build and test

```powershell
cmake --workflow --preset dev
```

This configures a Debug build, compiles the project, and runs all tests with
failure output enabled.

## Generate the Codeforces submission

```powershell
python scripts/generate_submission.py
g++ -std=c++2a -O2 -Wall -Wextra -Wpedantic submission.cpp -o submission.exe
Get-Content -Raw example.txt | ./submission.exe
```

`submission.cpp` is generated from the production headers and sources. Regenerate it after every source change.

## Simulate a policy

A scenario contains the normal two configuration lines and task-time table, followed by a request count and rows of:

```text
arrival_time input_length output_length
```

Run any policy against the exact local event model. Available policies are singleton, batch, score, and adaptive.
The score-aware policy accepts a HOT-set override:

```powershell
Get-Content -Raw tests/data/example1_scenario.txt |
  ./build/Debug/scheduler_simulator.exe --policy score --target-hot 8 --trace
Get-Content -Raw benchmarks/throughput_cohort.txt |
  ./build/Debug/scheduler_simulator.exe --policy adaptive
```

The trace reproduces every timestamp and the 500.000002759 score from statement example 1.

## Compare policies

The benchmark executable runs each policy module independently through the same simulator and prints CSV metrics:

```powershell
./build/Debug/scheduler_benchmark.exe benchmarks/decode_burst.txt
./build/Debug/scheduler_benchmark.exe benchmarks/waiting_pressure.txt
./build/Debug/scheduler_benchmark.exe benchmarks/waiting_pressure.txt --target-hot 4
./build/Release/scheduler_benchmark.exe benchmarks/throughput_cohort.txt
./build/Release/scheduler_benchmark.exe benchmarks/hard_slo.txt
```
