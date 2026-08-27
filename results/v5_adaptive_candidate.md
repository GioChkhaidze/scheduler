# V5 adaptive candidate

Version: V5-adaptive
Judge status: 22/22 OK
Submission: baselines/v5_adaptive_submission.cpp
SHA-256: e81772440c54ac34cb6ea9eaac64dcaf855fdc0de22b05f30e0438f942e372dd
Regression status: all eight Debug CTest targets pass
Compiler status: generated submission passes GCC with -O2 -Wall -Wextra -Wpedantic
Arithmetic mean: 643.4148063443
Total points: 14155.1257395745
Change from V2 mean: +46.2445205029
Aggregate judge runtime: 5317 ms

## Policy

V5 selects one of four score regimes entirely from the supplied configuration:

1. HardSlo uses a small active decode set when dist_base is zero and waiting has meaningful weight.
2. Waiting reuses the V4 HOT/COLD policy.
3. Balanced uses the V2 batching policy exactly, preventing the measured V4 regression on test 22.
4. Throughput builds and pipelines cohorts while limiting each stage to a modeled efficient batch size.

The throughput model estimates edge, cloud, and directional-link capacities for candidate batch sizes. It chooses
the smallest size estimated to reach tp_UB with a score-weighted safety margin. If no size reaches the target, it
uses the capacity-maximizing size. Four cohorts may remain active so edge, link, and cloud stages can overlap.

A throughput cohort waits only for work already known to be progressing. It never waits for hypothetical future
arrivals, so the policy retains a legal progress path when no future event is scheduled.

## Local comparisons

| Scenario | V2 score | V5 score | Delta | Key result |
|---|---:|---:|---:|---|
| statement example 1 | 500.000003 | 500.000003 | 0.000000 | exact control |
| decode burst | 626.582277 | 626.582277 | 0.000000 | exact balanced fallback |
| waiting pressure | 194.599135 | 223.979179 | +29.380043 | V4 waiting behavior |
| throughput cohort | 363.321871 | 465.835640 | +102.513770 | tp +28.21% |
| throughput short | 243.046914 | 260.959638 | +17.912724 | frames -35.20% |
| throughput steady | 608.059573 | 627.022516 | +18.962943 | TDR -53.72% |
| throughput mixed | 459.705806 | 513.880105 | +54.174298 | tp +20.23% |
| hard SLO | 0.000000 | 1000.000000 | +1000.000000 | both binary SLOs crossed |

The hard-SLO result is intentionally a near-threshold test. The smaller HOT set traded throughput for TPOT and
earned full waiting credit only because both supplied limits were met.

## Correctness and risk controls

1. V1, V2, V4, and V5 remain separate policy modules over the same state-transition core.
2. Balanced V5 decisions are tested for exact equality with V2 decisions.
3. Waiting V5 decisions are tested for exact equality with V4 decisions.
4. Cohort accumulation, cloud priority, admission limits, target saturation, and hard-SLO crossing are tested.
5. Completion sweeps cover K equal to 1, 4, and 8, burst and staggered arrivals, mixed output lengths, and
   throughput weights of 0.75 and 1.0.
6. Piecewise-linear interpolation now uses binary search with unchanged numerical behavior.
7. The generated and frozen artifacts have identical SHA-256 hashes.

## Judge comparison against V2

V5 improved four tests, regressed three tests, and was exactly unchanged on fifteen tests. The largest gains were
test 19 (+876.584673) and test 5 (+273.529704). Test 19 runtime fell from 11218 ms to 421 ms. The largest
regression was test 13 (-118.179860), where the 0.75 throughput weight triggered cohort mode too aggressively.

| Test | Runtime (ms) | Points | Delta vs V2 | tp | mean_tdr | mean_tpot | norm_tp | norm_c |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 15 | 500.0000027586 | 0.0000000000 | 0.022222 | 30.000000 | 0.000000 | 0.000000 | 1.000000 |
| 2 | 31 | 500.0000000000 | 0.0000000000 | 0.005755 | 126.158679 | 0.000000 | 0.000000 | 1.000000 |
| 3 | 15 | 0.0000000000 | 0.0000000000 | 0.004520 | 1355.547361 | 133.883029 | 0.658129 | 0.000000 |
| 4 | 15 | 725.4420161333 | 0.0000000000 | 0.039964 | 633.956118 | 125.358930 | 0.293852 | 0.910409 |
| 5 | 140 | 473.0069547622 | 273.5297037625 | 1.151615 | 1497.254452 | 66.843943 | 0.341825 | 0.997736 |
| 6 | 62 | 352.0710167623 | -18.6426992169 | 0.609192 | 3102.231970 | 67.498436 | 0.280962 | 0.992051 |
| 7 | 0 | 907.5289390182 | 0.0000000000 | 0.014412 | 860.122989 | 67.965521 | 0.420035 | 0.907529 |
| 8 | 31 | 764.6543466700 | 0.0000000000 | 0.010756 | 1211.580506 | 130.700128 | 0.580379 | 0.826080 |
| 9 | 31 | 679.0798623341 | 0.0000000000 | 0.003820 | 6670.031821 | 0.000000 | 0.777425 | 0.673904 |
| 10 | 125 | 567.3310392996 | 0.0000000000 | 0.006204 | 224370.368594 | 299.610523 | 0.698551 | 0.544175 |
| 11 | 46 | 500.1853822130 | 0.0000000000 | 0.000007 | 32780482.884393 | 16199.089335 | 0.000371 | 1.000000 |
| 12 | 46 | 805.7459043497 | 5.6146363921 | 0.000024 | 1382425.696096 | 211.768623 | 0.809081 | 0.475566 |
| 13 | 15 | 558.2144006425 | -118.1798596984 | 0.021365 | 2116.973081 | 159.646589 | 0.489230 | 0.765169 |
| 14 | 93 | 415.2668658781 | 0.0000000000 | 0.003564 | 192.489397 | 184.378198 | 0.210323 | 0.795876 |
| 15 | 46 | 714.5489915731 | 0.0000000000 | 0.000009 | 19314480.365600 | 0.000000 | 0.982404 | 0.495395 |
| 16 | 250 | 781.7598026736 | -1.5270036024 | 0.024251 | 47242.700263 | 633.323884 | 0.779380 | 0.898350 |
| 17 | 1843 | 833.4554050834 | 0.0000000000 | 0.000520 | 29116636.517908 | 25478.000147 | 0.986650 | 0.522423 |
| 18 | 218 | 784.1173342212 | 0.0000000000 | 0.000009 | 46957956.216119 | 0.000000 | 0.991025 | 0.498388 |
| 19 | 421 | 878.5308735049 | 876.5846734271 | 0.657394 | 165.183458 | 179.880062 | 0.878531 | 0.999938 |
| 20 | 921 | 998.2030007308 | 0.0000000000 | 0.005607 | 1257.782370 | 172.089175 | 0.995271 | 1.000000 |
| 21 | 375 | 605.1130294396 | 0.0000000000 | 0.004867 | 69127.153255 | 0.000000 | 0.309764 | 0.900462 |
| 22 | 578 | 810.8705715263 | 0.0000000000 | 27.390488 | 2780.006472 | 11.072111 | 0.626360 | 0.995381 |

The measured total is 1844.874260 points below the 16000 target. The next candidate must recover test 13 with a
more conservative regime boundary and improve additional bottlenecks; merely retuning HOT concurrency is unlikely
to close the remaining gap.
