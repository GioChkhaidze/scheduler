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
Competition output is compacted automatically and generation fails if it exceeds Codeforces' 65,535-character field
limit. Pass `--readable` when you want an uncompressed generated file for inspection.
Generate isolated V7 experiments without editing production code:

```powershell
python scripts/generate_submission.py --v7-stage tracking --output submission_v7_tracking.cpp
python scripts/generate_submission.py --v7-stage locality --output submission_v7_locality.cpp
python scripts/generate_submission.py --v7-stage up --output submission_v7_up.cpp
python scripts/generate_submission.py --v7-stage down --output submission_v7_down.cpp
python scripts/generate_submission.py --v7-stage combined --output submission.cpp
```

## Simulate a policy

A scenario contains the normal two configuration lines and task-time table, followed by a request count and rows of:

```text
arrival_time input_length output_length
```

Run any policy against the exact local event model. Available policies are singleton, batch, score, adaptive,
multiprocessor, v7-track, v7-locality, v7-up, v7-down, and v7. The staged V7 names isolate exact FIFO tracking,
D PRE remote locality, prefill UP admission, prefill DOWN admission, and the combined policy.
The score-aware policy accepts a HOT-set override:

```powershell
Get-Content -Raw tests/data/example1_scenario.txt |
  ./build/Debug/scheduler_simulator.exe --policy score --target-hot 8 --trace
Get-Content -Raw benchmarks/throughput_cohort.txt |
  ./build/Debug/scheduler_simulator.exe --policy adaptive
Get-Content -Raw benchmarks/decode_burst.txt |
  ./build/Debug/scheduler_simulator.exe --policy v7
```

The trace reproduces every timestamp and the 500.000002759 score from statement example 1.

## Compare policies

The benchmark executable runs every policy and each V7 experiment independently through the same simulator. Its CSV
also reports link reconciliation count and maximum timestamp error, so queue-model correctness is checked beside score:

```powershell
./build/Debug/scheduler_benchmark.exe benchmarks/decode_burst.txt
./build/Debug/scheduler_benchmark.exe benchmarks/waiting_pressure.txt
./build/Debug/scheduler_benchmark.exe benchmarks/waiting_pressure.txt --target-hot 4
./build/Release/scheduler_benchmark.exe benchmarks/throughput_cohort.txt
./build/Release/scheduler_benchmark.exe benchmarks/hard_slo.txt
```
