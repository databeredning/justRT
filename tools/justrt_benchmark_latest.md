Benchmark enabled: `1`
Cycle frequency: `120,000,000` Hz

| Task | Releases | Completions | Pending | Coalesced | Release max | Execution max | Budget | Stack Usage | Status |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| benchmark-a | 17,605 | 17,605 | 0 | 0 | 652 µs | 232 µs | 87.2% | 19/128 (14.8%) | ✓ PASS |
| benchmark-b | 17,601 | 17,600 | 1 | 0 | 396 µs | 87 µs | 21.9% | 19/128 (14.8%) | ✓ PASS |
| benchmark-c | 11,733 | 11,733 | 0 | 0 | 359 µs | 125 µs | 18.8% | 19/128 (14.8%) | ✓ PASS |
| benchmark-controller | 46,919 | 46,920 | 0 | 0 | 157 µs | 241 µs | — | 46/128 (35.9%) | ✓ PASS |
| benchmark-receiver | 11,733 | 11,733 | 0 | 11,733 | 485 µs | 262 µs | — | 29/128 (22.7%) | ⚠️ CHECK |
| benchmark-sender | 11,733 | 11,733 | 0 | 0 | 35 µs | 76 µs | — | 27/128 (21.1%) | ✓ PASS |
| timer-service | 0 | 0 | 0 | 0 | 0 µs | 0 µs | — | 17/128 (13.3%) | ✓ PASS |
