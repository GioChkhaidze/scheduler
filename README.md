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
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic submission.cpp -o submission.exe
Get-Content -Raw example.txt | ./submission.exe
```

`submission.cpp` is generated from the production headers and sources. Regenerate it after every source change.
