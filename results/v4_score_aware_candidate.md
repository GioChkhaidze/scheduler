# V4 score-aware candidate

Version: V4-score-aware
Judge status: 22/22 OK
Submission: baselines/v4_score_aware_submission.cpp
SHA-256: 1c42b060aefd236455e3d728ccb75adb33438ca4f27e1867d6f1eba2122552b6
Regression status: all seven CTest targets pass
Compiler status: GCC with full warnings passes
Arithmetic mean: 591.0535163922
Total points: 13003.1773606284
Change from V2 mean: -6.1167694492

## Policy

V4 explicitly classifies each request as PREFILL, COLD, HOT, or FINISHED. It ranks ready HOT work by normalized
TPOT urgency and ready COLD work by age. P POST wins when its normalized TDR excess is at least the actionable
HOT TPOT excess. Otherwise HOT decode continues first.

The configurable HOT target counts requests that already produced tokens and COLD first-token work in flight.
The default target is derived from scoring weight, decode-cycle time, TPOT slack, cloud count, and efficient batch
sizes from the task curves. The --target-hot option overrides it in local simulation and benchmarking.

## Local comparisons

| Scenario | Policy | Target | Frames | tp | mean_tdr | mean_tpot | Score |
|---|---|---:|---:|---:|---:|---:|---:|
| statement example 1 | V2 decode batching | n/a | 11 | 0.022222222 | 30.000000 | 0.000000 | 500.000003 |
| statement example 1 | V4 score aware | 18 | 11 | 0.022222222 | 30.000000 | 0.000000 | 500.000003 |
| decode burst | V2 decode batching | n/a | 204 | 0.253164554 | 147.673438 | 32.850001 | 626.582277 |
| decode burst | V4 score aware | 144 | 204 | 0.253164554 | 147.673438 | 32.850001 | 626.582277 |
| waiting pressure | V2 decode batching | n/a | 204 | 0.253164554 | 147.673438 | 32.850001 | 194.599135 |
| waiting pressure | V4 score aware | 10 | 222 | 0.221290571 | 147.354688 | 31.428125 | 223.979179 |

The waiting-pressure improvement is +29.380043 points. The decode-burst control remains identical to V2.

## Judge comparison against V2

V4 remained exactly equal to V2 on 20 of 22 tests. It improved test 17 by 0.242724 points, but regressed test
22 by 134.811652 points. The net change was -134.568928 total points, or -6.116769 points in the arithmetic
mean. Aggregate judge runtime increased from 15222 ms to 20975 ms.

| Test | V2 points | V4 points | Delta | Important metric change |
|---:|---:|---:|---:|---|
| 17 | 833.455405 | 833.698129 | +0.242724 | mean TPOT 25478.000147 -> 5918.715236 |
| 22 | 810.870572 | 676.058920 | -134.811652 | tp 27.390488 -> 15.671351; mean TPOT 11.072111 -> 9.345745 |

Test 22 exposes the policy's main failure: restricting COLD admission improved TPOT by 15.59%, but reduced
throughput by 42.79%. TDR and distance were effectively unchanged. The next policy must preserve V2-style
wide admission on throughput-heavy tests and apply HOT-set throttling only when the scoring parameters make
the TPOT benefit worth the throughput loss.

## Full judge vector

| Test | Runtime (ms) | Points | Delta vs V2 | tp | mean_tdr | mean_tpot | dist | norm_tp | norm_c | normalized_score |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0 | 500.0000027586 | 0.0000000000 | 0.022222 | 30.000000 | 0.000000 | 0.000000 | 0.000000 | 1.000000 | 0.500000 |
| 2 | 15 | 500.0000000000 | 0.0000000000 | 0.005755 | 126.158679 | 0.000000 | 0.000000 | 0.000000 | 1.000000 | 0.500000 |
| 3 | 31 | 0.0000000000 | 0.0000000000 | 0.004520 | 1355.547361 | 133.883029 | 1.223757 | 0.658129 | 0.000000 | 0.000000 |
| 4 | 15 | 725.4420161333 | 0.0000000000 | 0.039964 | 633.956118 | 125.358930 | 2.432041 | 0.293852 | 0.910409 | 0.725442 |
| 5 | 828 | 199.4772509997 | 0.0000000000 | 0.139710 | 76009.031685 | 67.913372 | 244.492782 | 0.035431 | 0.855664 | 0.199477 |
| 6 | 109 | 370.7137159792 | 0.0000000000 | 0.652677 | 3352.596889 | 82.574890 | 5.645244 | 0.301763 | 0.991274 | 0.370714 |
| 7 | 0 | 907.5289390182 | 0.0000000000 | 0.014412 | 860.122989 | 67.965521 | 0.371524 | 0.420035 | 0.907529 | 0.907529 |
| 8 | 15 | 764.6543466700 | 0.0000000000 | 0.010756 | 1211.580506 | 130.700128 | 1.893088 | 0.580379 | 0.826080 | 0.764654 |
| 9 | 46 | 679.0798623341 | 0.0000000000 | 0.003820 | 6670.031821 | 0.000000 | 11.039101 | 0.777425 | 0.673904 | 0.679080 |
| 10 | 109 | 567.3310392996 | 0.0000000000 | 0.006204 | 224370.368594 | 299.610523 | 177.262230 | 0.698551 | 0.544175 | 0.567331 |
| 11 | 15 | 500.1853822130 | 0.0000000000 | 0.000007 | 32780482.884393 | 16199.089335 | 0.000000 | 0.000371 | 1.000000 | 0.500185 |
| 12 | 15 | 800.1312679576 | 0.0000000000 | 0.000024 | 1382425.696096 | 732.828168 | 5.315194 | 0.808213 | 0.000000 | 0.800131 |
| 13 | 15 | 676.3942603409 | 0.0000000000 | 0.025735 | 1825.714414 | 202.739551 | 3.876052 | 0.645028 | 0.770492 | 0.676394 |
| 14 | 125 | 415.2668658781 | 0.0000000000 | 0.003564 | 192.489397 | 184.378198 | 0.176642 | 0.210323 | 0.795876 | 0.415267 |
| 15 | 62 | 714.5489915731 | 0.0000000000 | 0.000009 | 19314480.365600 | 0.000000 | 90.995602 | 0.982404 | 0.495395 | 0.714549 |
| 16 | 140 | 783.2868062760 | 0.0000000000 | 0.024298 | 52010.055340 | 362.289247 | 44.326118 | 0.781123 | 0.889308 | 0.783287 |
| 17 | 3171 | 833.6981292108 | 0.2427241274 | 0.000520 | 29116636.517908 | 5918.715236 | 1553.997354 | 0.986635 | 0.523189 | 0.833698 |
| 18 | 250 | 784.1173342212 | 0.0000000000 | 0.000009 | 46957956.216119 | 0.000000 | 371.688376 | 0.991025 | 0.498388 | 0.784117 |
| 19 | 13953 | 1.9462000778 | 0.0000000000 | 0.015175 | 6418570.583897 | 130.999092 | 37377.218610 | 0.001946 | 0.096813 | 0.001946 |
| 20 | 968 | 998.2030007308 | 0.0000000000 | 0.005607 | 1257.782370 | 172.089175 | 0.000000 | 0.995271 | 1.000000 | 0.998203 |
| 21 | 234 | 605.1130294396 | 0.0000000000 | 0.004867 | 69127.153255 | 0.000000 | 290.442837 | 0.309764 | 0.900462 | 0.605113 |
| 22 | 859 | 676.0589195168 | -134.8116520095 | 15.671351 | 2780.006472 | 9.345745 | 369.542832 | 0.356737 | 0.995381 | 0.676059 |

## Correctness evidence

* Separate V1, V2, and V4 policy tests.
* Explicit classification tests for every lifecycle region.
* HOT-first, TDR-pressure, TPOT-pressure, and D POST precedence tests.
* First-token in-flight accounting and strict COLD admission-cap tests.
* Statement example reproduced through the exact event simulator.
* Synthetic completion sweep across K=1/2/8, num_layers=1/64, every scoring-weight regime, mixed arrivals, input
  lengths 1 through 4096, output lengths 1 through 4, and target HOT-set size 1.

This record corresponds to the exact frozen artifact and its reported 22-test judge vector.
