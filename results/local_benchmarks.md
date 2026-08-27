# Local policy benchmarks

The benchmark executable runs each policy module through the same event engine and workload. Simulated metrics are
deterministic; wall-clock microseconds are one Release-build observation and are not treated as a score oracle.

Current generated submission SHA-256 after the policy-module refactor:
`1abbfd323128c8e63ad0ff7bd83be9cc647a10ed57738a0049e03f688974e2bd`.
The judged V1 and V2 files remain frozen separately under `baselines/`.

## decode_burst.txt

| Policy | Completed | Frames | tp | mean_tdr | mean_tpot | Score | Wall time (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| singleton | 1 | 632 | 0.071756923 | 738.012500010 | 34.887500010 | 535.878461682 | 921 |
| decode_batch | 1 | 204 | 0.253164554 | 147.673437610 | 32.850000667 | 626.582276839 | 320 |

On this burst workload, decode batching cuts event frames by 67.7%, increases simulated throughput by 3.53x, and
raises the simulated score by 90.704 points.
