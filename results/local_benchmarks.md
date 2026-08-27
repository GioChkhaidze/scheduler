# Local policy benchmarks

The benchmark executable runs each policy module through the same event engine and workload. Simulated metrics are
deterministic; wall-clock microseconds are one Release-build observation and are not treated as a score oracle.

Current V5.1 generated submission SHA-256:
`31f815372fa0d55fee4c327df6c67f851f84f08b32c3ab1e38aeb9ea3e40a496`.
Every submitted or candidate version remains frozen separately under `baselines/`.

## decode_burst.txt

| Policy | Completed | Frames | tp | mean_tdr | mean_tpot | Score | Wall time (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| singleton | 1 | 632 | 0.071756923 | 738.012500010 | 34.887500010 | 535.878461682 | 921 |
| decode_batch | 1 | 204 | 0.253164554 | 147.673437610 | 32.850000667 | 626.582276839 | 320 |

On this burst workload, decode batching cuts event frames by 67.7%, increases simulated throughput by 3.53x, and
raises the simulated score by 90.704 points.

## V5 adaptive matrix

| Scenario | V2 score | V5 score | Delta | V2 frames | V5 frames |
|---|---:|---:|---:|---:|---:|
| statement example 1 | 500.000003 | 500.000003 | 0.000000 | 11 | 11 |
| decode burst | 626.582277 | 626.582277 | 0.000000 | 204 | 204 |
| waiting pressure | 194.599135 | 223.979179 | +29.380043 | 204 | 222 |
| throughput cohort | 363.321871 | 465.835640 | +102.513770 | 1109 | 632 |
| throughput short | 243.046914 | 262.918587 | +19.871672 | 483 | 298 |
| throughput steady | 608.059573 | 707.794806 | +99.735233 | 938 | 520 |
| throughput mixed | 459.705806 | 550.699050 | +90.993244 | 1261 | 674 |
| hard SLO | 0.000000 | 1000.000000 | +1000.000000 | 765 | 1816 |

These scenarios test policy mechanics and regression safety. Their score total is not a prediction of the hidden
preliminary vector.
