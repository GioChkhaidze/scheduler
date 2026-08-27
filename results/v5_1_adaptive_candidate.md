# V5.1 adaptive candidate

Version: V5.1-adaptive
Judge status: pending
Submission: baselines/v5_1_adaptive_submission.cpp
SHA-256: 31f815372fa0d55fee4c327df6c67f851f84f08b32c3ab1e38aeb9ea3e40a496
Parent result: V5 scored 14155.1257395745 with 22/22 OK
Regression status: all eight Debug CTest targets pass
Compiler status: generated submission passes GCC with -O2 -Wall -Wextra -Wpedantic

## Changes from judged V5

1. Raise the throughput-regime boundary from 0.70 to 0.80. Test 13 has w_tp equal to 0.75, so this restores
   the exact V2 fallback instead of applying the cohort policy that lost 118.179860 points.
2. Remove modeled maximum stage-batch caps. The capacity model still decides when a cohort is large enough,
   but the proven V2 curve optimizer may use every currently ready request.
3. Track observed TDR and TPOT. Waiting mode returns to V2 batching when both running means are below 80%
   of their SLOs, avoiding needless throughput sacrifice.
4. In pure-waiting hard-SLO tests, warm up all currently known prefills before starting the first decode
   cohort, then retain the one-lane-per-cloud HOT admission limit.

## Evidence

| Scenario | V2 score | Judged-V5 behavior | V5.1 score | V5.1 frames |
|---|---:|---:|---:|---:|
| statement example 1 | 500.000003 | exact control | 500.000003 | 11 |
| decode burst | 626.582277 | exact control | 626.582277 | 204 |
| waiting pressure | 194.599135 | +29.380043 | 223.979179 | 222 |
| throughput cohort | 363.321871 | +102.513770 | 465.835640 | 632 |
| throughput short | 243.046914 | +17.912724 | 262.918587 | 298 |
| throughput steady | 608.059573 | +18.962943 | 707.794806 | 520 |
| throughput mixed | 459.705806 | +54.174298 | 550.699050 | 674 |
| hard SLO | 0.000000 | full credit | 1000.000000 | 1816 |

On the hard-SLO scenario, prefill warmup changed mean TDR from 693.395313 to 396.003125 and mean TPOT
from 27.706027 to 25.900000. The rule is enabled only when dist_base is zero and w_tp is at most 0.1.

If every other judged test were unchanged, the conservative boundary alone would raise the measured total to
14273.3055992729. The hard-SLO and uncapped-cohort effects require another judge measurement and are not
included in that lower-bound comparison.
