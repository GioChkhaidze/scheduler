# V7 shared-link local validation

- Date recorded: 2026-08-28
- Control: V6 multiprocessor, 14684.8439606496 judged total, 22/22 OK
- Combined V7 submission SHA-256: `54716588ac9e7d170eee2042e7db1b60dbe6ced4b9b6de83765cbf9b43aa2abf`
- Codeforces source size: 58861 characters, 6674 below the 65535-character limit
- Preliminary verdict: 22/22 OK
- Preliminary total: 14684.8439606496; delta from V6: 0
- Preliminary mean: 667.4929073023
- Aggregate reported runtime: 5833 ms
- Exact judged source: `baselines/v7_combined_judged_submission.cpp`
- Correctness suite: 10/10 CTest executables passed
- Single-file gate: tracking, locality, UP, DOWN, and combined variants all compile with GCC warnings enabled
- FIFO gate: every local XDN reconciled; maximum predicted timestamp error was 0 on all benchmark scenarios
- Protocol gate: the generated combined submission completed `example.txt` with legal assignments

## Local score comparison

These scenarios are regression tests, not estimates of the hidden judge score. A score delta of zero still verifies that
instrumentation or a policy stage did not perturb the control on that workload.

| Scenario | V6 | Tracking | D PRE locality | UP admission | DOWN admission | Combined |
|---|---:|---:|---:|---:|---:|---:|
| decode_burst | 626.582277 | 626.582277 | 626.582277 | 626.582277 | 626.582277 | 626.582277 |
| hard_slo | 1000.000000 | 1000.000000 | 1000.000000 | 1000.000000 | 1000.000000 | 1000.000000 |
| prefill_imbalance | 513.635247 | 513.635247 | 513.635247 | 513.635247 | 513.635247 | 513.635247 |
| throughput_cohort | 465.835640 | 465.835640 | 465.835640 | 465.835640 | 465.835640 | 465.835640 |
| throughput_mixed | 550.699050 | 550.699050 | 550.699050 | 550.699050 | 550.699050 | 550.699050 |
| throughput_short | 262.918587 | 262.918587 | 262.918587 | 262.918587 | 262.918587 | 262.918587 |
| throughput_steady | 707.794806 | 707.794806 | 707.794806 | 707.794806 | 707.794806 | 707.794806 |
| waiting_pressure | 223.979179 | 223.979179 | 223.979179 | 223.979179 | 223.979179 | 223.979179 |

On `hard_slo`, locality reduced link reconciliations from 1152 to 384 and mean TPOT from 25.900000160 to
21.200000640 while the score remained saturated at 1000.

## Preliminary-judge experiment log

Submit in this order and paste every 22-test vector before enabling the next mechanism.

| Stage | Source command | Verdict | Total | Delta vs V6 | Test 10 | Test 21 |
|---|---|---:|---:|---:|---:|---:|
| V7.0 tracking | `python scripts/generate_submission.py --v7-stage tracking` | pending | | | | |
| V7.1 locality | `python scripts/generate_submission.py --v7-stage locality` | pending | | | | |
| V7.2 UP | `python scripts/generate_submission.py --v7-stage up` | pending | | | | |
| V7.2 DOWN | `python scripts/generate_submission.py --v7-stage down` | pending | | | | |
| Combined V7 | `python scripts/generate_submission.py --v7-stage combined` | 22/22 OK | 14684.8439606496 | 0 | 566.9765903376 | 924.2879907645 |

Every test from 1 through 22 had a point delta of exactly 0. Every reported TP, mean TDR, mean TPOT, `dist`, normalized
throughput, and normalized cost value was also identical to V6. Test 21's 924.2879907645 gain was preserved, while
test 10 remained at 566.9765903376.

This is strong evidence that no V7 policy trigger changed an assignment on the preliminary workloads. Tracking is
deliberately observational. Locality requires a compatible ready filler outside the V6 batch on an already represented
remote. UP/DOWN admission requires a known HOT transfer whose projected SLO violation exceeds competing prefill TDR
pressure. Those latter conditions did not arise in the 22 preliminary tests.
