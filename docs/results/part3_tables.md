# Part 3 case-study tables

## 1. Bellman-Ford: distance to vertex 100

| graph | 10 → 100 | 20 → 100 | 30 → 100 |
|---|---|---|---|
| grafo_W_1 | −∞ (negative cycle) | −∞ (negative cycle) | −∞ (negative cycle) |
| grafo_W_2 | 10.47 | 8.48 | 8.71 |
| grafo_W_3 | 4.63 | 5.42 | 4.67 |
| grafo_W_4 | 9.04 | 8.83 | 9.23 |
| grafo_W_5 | 37.85 | 42.63 | 37.86 |

## 2. Floyd-Warshall: distances among {10, 20, 30, 40, 50}

### grafo_W_1

| from \ to | 10 | 20 | 30 | 40 | 50 |
|---|---|---|---|---|---|
| 10 | — | −∞ (negative cycle) | −∞ (negative cycle) | −∞ (negative cycle) | −∞ (negative cycle) |
| 20 | −∞ (negative cycle) | — | −∞ (negative cycle) | −∞ (negative cycle) | −∞ (negative cycle) |
| 30 | −∞ (negative cycle) | −∞ (negative cycle) | — | −∞ (negative cycle) | −∞ (negative cycle) |
| 40 | −∞ (negative cycle) | −∞ (negative cycle) | −∞ (negative cycle) | — | −∞ (negative cycle) |
| 50 | −∞ (negative cycle) | −∞ (negative cycle) | −∞ (negative cycle) | −∞ (negative cycle) | — |

### grafo_W_2

| from \ to | 10 | 20 | 30 | 40 | 50 |
|---|---|---|---|---|---|
| 10 | — | 11.73 | 8.14 | 9.49 | 7.08 |
| 20 | 3.84 | — | 7.03 | 8.30 | 4.70 |
| 30 | 5.79 | 12.55 | — | 8.30 | 6.33 |
| 40 | 5.05 | 12.32 | 5.36 | — | 5.71 |
| 50 | 4.48 | 12.23 | 7.79 | 8.26 | — |

### grafo_W_3 (via Bellman-Ford toward each vertex: Floyd-Warshall needs 120 GB, no_memory)

| from \ to | 10 | 20 | 30 | 40 | 50 |
|---|---|---|---|---|---|
| 10 | — | 4.99 | 5.71 | 7.49 | 4.31 |
| 20 | 3.48 | — | 6.19 | 8.27 | 4.32 |
| 30 | 3.96 | 5.80 | — | 8.30 | 3.17 |
| 40 | 3.58 | 5.59 | 6.10 | — | 3.84 |
| 50 | 3.25 | 5.41 | 6.12 | 8.10 | — |

### grafo_W_4 (via Bellman-Ford toward each vertex: Floyd-Warshall needs 12,001 GB, no_memory)

| from \ to | 10 | 20 | 30 | 40 | 50 |
|---|---|---|---|---|---|
| 10 | — | 10.52 | 10.15 | 10.69 | 8.24 |
| 20 | 7.79 | — | 9.19 | 9.67 | 7.58 |
| 30 | 7.61 | 10.25 | — | 8.77 | 6.55 |
| 40 | 7.94 | 8.65 | 9.22 | — | 7.75 |
| 50 | 7.99 | 11.41 | 8.80 | 8.55 | — |

### grafo_W_5 (via Bellman-Ford toward each vertex: Floyd-Warshall needs 1,200,001 GB, no_memory)

| from \ to | 10 | 20 | 30 | 40 | 50 |
|---|---|---|---|---|---|
| 10 | — | 40.20 | 39.56 | 41.55 | 44.91 |
| 20 | 39.17 | — | 39.98 | 49.82 | 45.16 |
| 30 | 34.46 | 42.64 | — | 47.04 | 43.85 |
| 40 | 38.73 | 43.91 | 47.30 | — | 45.24 |
| 50 | 44.06 | 40.62 | 45.25 | 49.11 | — |


## 3. Mean running time (seconds, without disk I/O)

| graph | Bellman-Ford | Floyd-Warshall | negative-cycle check |
|---|---|---|---|
| grafo_W_1 | 88.4 (10 runs) | 1.09e+04 (1 run) | 93.7 (10 runs) |
| grafo_W_2 | 0.0319 (10 runs) | 1.01e+04 (1 run) | 8e-08 (10 runs) |
| grafo_W_3 | 0.487 (10 runs) | ≈ 6.69e+05[^1] | 1e-07 (10 runs) |
| grafo_W_4 | 4.8 (10 runs) | ≈ 6.69e+08[^2] | 1e-07 (10 runs) |
| grafo_W_5 | 16 (10 runs) | ≈ 6.69e+11[^3] | 4e-08 (10 runs) |

## 4. All pairs: n × Bellman-Ford against Floyd-Warshall

| graph | all-pairs Bellman-Ford (s) | Floyd-Warshall (s) |
|---|---|---|
| grafo_W_1 | ≈ 2.32e+06[^4] | 1.09e+04 (1 run) |
| grafo_W_2 | 466 (measured) | 1.01e+04 (1 run) |
| grafo_W_3 | ≈ 4.03e+04[^5] | ≈ 6.69e+05[^6] |
| grafo_W_4 | ≈ 3.5e+06[^7] | ≈ 6.69e+08[^8] |
| grafo_W_5 | ≈ 1.38e+08[^9] | ≈ 6.69e+11[^10] |

[^1]: grafo_W_3, floyd_warshall: c * n^3, c = 6.69488e-10 s measured on grafo_W_1, grafo_W_2; the random-graph calibration gave c = 1.24087e-09 s
[^2]: grafo_W_4, floyd_warshall: c * n^3, c = 6.69488e-10 s measured on grafo_W_1, grafo_W_2; the random-graph calibration gave c = 8.42362e-10 s
[^3]: grafo_W_5, floyd_warshall: c * n^3, c = 6.69488e-10 s measured on grafo_W_1, grafo_W_2; the random-graph calibration gave c = 6.76023e-10 s
[^4]: grafo_W_1, all-pairs Bellman-Ford: reversal + n x mean pass = 0.02267 + 25000 x 92.7774 s (3 targets measured)
[^5]: grafo_W_3, all-pairs Bellman-Ford: reversal + n x mean pass = 0.371608 + 100000 x 0.403216 s (100 targets measured)
[^6]: grafo_W_3, floyd_warshall: c * n^3, c = 6.69488e-10 s measured on grafo_W_1, grafo_W_2; the random-graph calibration gave c = 1.24087e-09 s
[^7]: grafo_W_4, all-pairs Bellman-Ford: reversal + n x mean pass = 2.10208 + 1000000 x 3.49897 s (33 targets measured)
[^8]: grafo_W_4, floyd_warshall: c * n^3, c = 6.69488e-10 s measured on grafo_W_1, grafo_W_2; the random-graph calibration gave c = 8.42362e-10 s
[^9]: grafo_W_5, all-pairs Bellman-Ford: reversal + n x mean pass = 3.93076 + 10000000 x 13.825 s (8 targets measured)
[^10]: grafo_W_5, floyd_warshall: c * n^3, c = 6.69488e-10 s measured on grafo_W_1, grafo_W_2; the random-graph calibration gave c = 6.76023e-10 s
