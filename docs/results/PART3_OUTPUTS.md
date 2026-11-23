# Part 3 outputs: what each file holds and what it means

This page explains the files that the Part 3 case studies produced: where
they are, what each row and column means, and how the numbers in
[part3_tables.md](part3_tables.md) come from them. Read it before quoting a
number in the report.

## 1. Where the results are

| folder | what it is |
|---|---|
| `data/azure_part3/` | First Azure run (`config_part3.csv`, three harness processes at once). Holds the Floyd-Warshall runs on W_1 and W_2, the calibration, the Bellman-Ford variants and the all-pairs samples. |
| `data/azure_part3_followup/` | Follow-up Azure run (`config_part3_followup.csv`, one process). 10 runs of Bellman-Ford and of the negative-cycle check on every graph, the full all-pairs Bellman-Ford on W_2, and the distances among {10, 20, 30, 40, 50} by Bellman-Ford on W_3 to W_5. |
| `data/azure_part3_final/` | **The final dataset: quote this one.** Built by `analysis/combine_part3.py` from the two runs above. |
| `docs/results/part3_tables.md` | The four tables of the assignment, generated from the final dataset by `analysis/report_part3.py --results data/azure_part3_final`. |

Each run folder holds:

- `part3_runs/<graph>/<launch>.csv`: the raw output of each launch of the
  program (§3);
- `results_part3.csv` and `paths_part3.csv`: the merged results (§4, §5);
- `environment.txt`: the VM, the CPU, the compiler and the commit;
- `run*.log`: the harness logs.

All the runs took place on one Azure VM: `Standard_E4as_v4` (4 vCPU, 32 GiB),
in `belgiumcentral`. The program reads the files from disk first, and every
time reported excludes that reading (assignment item 3: "sem considerar o
tempo de leitura do arquivo").

### How the final dataset is built

`analysis/combine_part3.py` takes these steps:

1. It copies the first run's `part3_runs/`.
2. It replaces each launch file with the follow-up file of the same name, if
   there is one: `bf.csv`, `cycle.csv`, W_2's `allpairs.csv`, and the
   `bf_check_*.csv` files of W_3 to W_5.
3. In `bf.csv`, it keeps the first run's variant rows (`bellman_ford_pass_<variant>`,
   `rounds_<variant>`, `relaxations_<variant>`), because the follow-up did not
   rerun the variants.
4. It merges the result with `config_part3.csv`, setting `runs = 10` everywhere,
   so that any step with fewer than 10 samples is marked `fewer_runs`.

The two runs give the same answers. `analysis/compare_part3.py` found
0 differences in every distance, path and negative cycle they share. Only the
timings differ between them.

## 2. Weights and distances

The program reads each weight as an exact integer: "1.25" with 2 decimals is
stored as 125. Every sum of weights is therefore exact, with no floating-point
rounding. The program divides by 100 again when it prints. A distance in any
output is in the units of the input file, with 2 decimals.

| printed | meaning |
|---|---|
| `10.47` | shortest-path distance |
| `inf` | no path (status `unreachable`) |
| `-inf` | a negative cycle lies on some path between the two vertices, so the distance is undefined (status `negative_cycle`) |

## 3. Raw launch files: `part3_runs/<graph>/<launch>.csv`

The harness starts `build/benchmark_part3` once per *launch*, and each launch
writes one file. A file counts only if its last row is `done,<mode>`. The
merge ignores an incomplete file, and a restart renames it.

| launch file | mode | what it measures |
|---|---|---|
| `cycle.csv` | `cycle` | `find_negative_cycle` on the whole graph: is there any negative cycle, and which one |
| `bf.csv` | `bf` | Bellman-Ford toward target 100 from sources 10, 20 and 30 (case study 1), plus the three variants when the config asks for them |
| `fw.csv` | `fw` | Floyd-Warshall, with the distances among {10, 20, 30, 40, 50} (case study 2) |
| `bf_check_<t>.csv` | `bf`, 1 run | Bellman-Ford toward t from the set. It cross-checks Floyd-Warshall on W_1 and W_2, and replaces Floyd-Warshall on W_3 to W_5 |
| `allpairs.csv` | `allpairs` | All-pairs Bellman-Ford: one pass per target (case study 4) |
| `calibrate.csv` | `calibrate` | Floyd-Warshall on small random graphs, used to fit a cost per n³ |
| `fw_decision.txt` | — | written by the harness: `run`, `no_memory` or `skipped`, followed by the bytes that Floyd-Warshall needs |

### Row kinds

Every row starts with its kind.

| row | fields | meaning |
|---|---|---|
| `info` | `metric, value` | A fact about the input or the run. Keys: `vertices`, `edges`, `directed`, `skipped_self_loops`, `negative_self_loops`, `scale` (100), `budget_seconds`, `max_abs_weight`, `memory_bytes`, `negative_cycle`, `rounds`, `relaxations` (`*_<variant>` for the variants), `seed`, `k`, `targets_measured`, `allpairs_full`. |
| `time` | `algorithm, run, micros[, param]` | One timed run, in microseconds. `reverse` is the time to build the reversed graph. `bellman_ford_pass` is one Bellman-Ford pass on it. `bellman_ford_pass_<variant>` is one pass of a variant. `floyd_warshall` is one Floyd-Warshall run. `negative_cycle_check` is one call of `find_negative_cycle`. `allpairs_pass` is one pass toward one target, with the target in `param`. |
| `calib` | `floyd_warshall, n, arcs, run, micros` | One calibration run on a random graph |
| `fit` | `floyd_warshall_n3, c, points` | The least-squares fit of time ≈ c · n³ over the calibration runs |
| `progress` | `algorithm, k, elapsed_micros, rows` | A heartbeat during long runs (every 250 pivots of Floyd-Warshall). The watchdog uses it, and the merge ignores it. |
| `path` | `algorithm, source, target, distance, hops, path, status` | One answer: the distance, the number of arcs, and the vertices separated by spaces (source first) |
| `cycle` | `found, length, weight, vertices, status` | The negative cycle found (`yes`/`no`), its arc count, its total weight and its vertices |
| `check` | `witness, status, detail` | The program checks the reported cycle against the graph: every arc exists and the weight is negative. `ok` or `error`. |
| `done` | `mode` | The launch finished |

### How many runs: the time budget

Each timed step aims for `runs` samples (10) and has a time budget
(`budget_seconds`). Run 1 always takes place. Each further run takes place
while fewer than `runs` samples exist **and** the time used so far plus the
longest run so far still fits in the budget. A slow step therefore stops
early, and the merge marks it `fewer_runs`, with the actual count in
`samples`.

## 4. `results_part3.csv`

Columns: `graph, mode, algorithm, metric, value, samples, status, note`.

- `mode` is the launch the row came from (`cycle`, `bf`, `fw`, `allpairs`), or
  `-` for a fact about the graph.
- `samples` is the number of timed runs behind a time. For
  `targets_measured` and `total_s`, it is the number of targets.
- Times are in **seconds**.

| mode | algorithm | metric | meaning |
|---|---|---|---|
| `-` | `-` | `max_abs_weight` | Largest weight in absolute value |
| `-` | `matrix` | `representation` | Always `skipped`: Part 3 runs on the CSR only |
| `cycle` | `negative_cycle_check` | `mean_s`, `median_s` | Time of `find_negative_cycle` |
| `cycle` | `negative_cycle_check` | `negative_cycle` | `yes`/`no`; `note` has the weight and the vertices of the witness |
| `bf` | `bellman_ford` | `mean_s`, `median_s` | **Case study 3, Bellman-Ford.** Reversal plus one pass, per run. |
| `bf` | `bellman_ford` | `reverse_mean_s` | The reversal part alone |
| `bf` | `bellman_ford` | `rounds`, `relaxations` | Rounds run and arcs relaxed by the default variant (`frontier`, both optimizations) |
| `bf` | `bellman_ford_<variant>` | `mean_s`, `median_s`, `rounds`, `relaxations` | The same for each variant, timing the pass only: `textbook` (n − 1 full rounds), `stop_when_stable` (optimization 1: stop after a round with no change), `frontier` (optimizations 1 and 2: each round relaxes only the arcs of the vertices that changed in the previous round) |
| `fw` | `floyd_warshall` | `mean_s`, `median_s` | **Case study 3, Floyd-Warshall** |
| `fw` | `floyd_warshall` | `memory_bytes` | (n + 1)² × 12 bytes: an 8-byte distance and a 4-byte predecessor per cell. Status `no_memory` when the VM cannot hold it |
| `fw` | `floyd_warshall` | `estimate_s` | Estimated time where Floyd-Warshall did not run: c · n³ (§6) |
| `fw` | `floyd_warshall` | `fw_vs_bf` | `ok` if every distance among the set matches the `bf_check` launches exactly; `mismatch` lists the pairs that differ |
| `allpairs` | `bellman_ford` | `total_s` | **Case study 4**, measured in full: the reversal plus all n passes |
| `allpairs` | `bellman_ford` | `estimate_s` | Case study 4, extrapolated: reversal + n × the mean pass (§6) |
| `allpairs` | `bellman_ford` | `targets_measured` | Passes actually timed |
| `allpairs` | `bellman_ford` | `pass_spread_s` | Standard deviation of one pass across targets |

### Statuses

| status | meaning |
|---|---|
| `ok` | Measured, with all the runs asked for |
| `fewer_runs` | Measured, but the budget allowed fewer than 10 runs (`samples` gives the count) |
| `extrapolated` | Not measured: an estimate, explained in `note` |
| `no_memory` | Floyd-Warshall's matrices do not fit in the VM's memory |
| `skipped` | Not run, by design (`note` says why) |
| `mismatch` | The Floyd-Warshall and Bellman-Ford answers differ (none occurred) |
| `error` | A launch failed or a file is missing (none occurred) |

## 5. `paths_part3.csv`

Columns: `graph, algorithm, source, target, distance, hops, path, status`.

| algorithm | rows | case study |
|---|---|---|
| `bellman_ford` | 10, 20, 30 → 100 | 1 |
| `floyd_warshall` | the 20 ordered pairs of {10, 20, 30, 40, 50} (W_1, W_2) | 2 |
| `bellman_ford_set` | the same 20 pairs, by Bellman-Ford toward each vertex of the set (W_3 to W_5, where Floyd-Warshall does not fit in memory) | 2, substitute |

Path statuses:

| status | meaning |
|---|---|
| `ok` | A path exists. `distance`, `hops` and `path` are filled in. |
| `unreachable` | No path. The distance is `inf` and the path is empty. |
| `negative_cycle` | The distance is `-inf` and the path is empty, because no shortest path exists |
| `out_of_range` | The vertex id is not in the graph |

All 115 rows of the final dataset are `ok` (92) or `negative_cycle` (23, all
on W_1).

## 6. Estimates: how they are computed

**Floyd-Warshall.** It does about n³ updates of the matrix, so the estimate
is time ≈ c · n³. Here c = 6.69 × 10⁻¹⁰ s per n³, the mean of the two real
runs:

- W_1: 10,854 s / 25,000³;
- W_2: 10,068 s / 25,000³.

The calibration on small random graphs gave c between 6.8 × 10⁻¹⁰ and
1.2 × 10⁻⁹ s, depending on the graph. The merge prefers the c measured on the
real case-study graphs, and `note` records both values.

**All-pairs Bellman-Ford.** Every target needs one pass on the same reversed
graph, so the estimate is the reversal time + n × the mean pass. The mean pass
comes from `targets_measured` targets drawn with seed 2025. W_2 was measured
in full: 25,000 passes, 465.5 s.

## 7. What was not run, and why

The assignment asks for 10 runs of each algorithm (item 3) and a comparison of
all pairs (item 4). The VM, a 32 GiB machine with the budget the project had,
could not run the steps below, and the estimates in §6 justify each drop.

| step | graph | needed | why it was dropped |
|---|---|---|---|
| Floyd-Warshall, 10 runs | W_1 | ≈ 30 h (10 × 10,854 s) | 1 run was measured; 9 more add about 27 h |
| Floyd-Warshall, 10 runs | W_2 | ≈ 28 h (10 × 10,068 s) | the same |
| Floyd-Warshall | W_3 | 120 GB of memory, ≈ 7.7 days | does not fit in 32 GiB |
| Floyd-Warshall | W_4 | 12 TB of memory, ≈ 21 years | does not fit |
| Floyd-Warshall | W_5 | 1.2 PB of memory, ≈ 21,000 years | does not fit |
| all-pairs Bellman-Ford, in full | W_1 | ≈ 27 days (25,000 × 92.8 s) | the negative cycle makes every pass run all n − 1 rounds |
| all-pairs Bellman-Ford, in full | W_3 | ≈ 11 h (100,000 × 0.40 s) | over the time budget |
| all-pairs Bellman-Ford, in full | W_4 | ≈ 40 days | over the time budget |
| all-pairs Bellman-Ford, in full | W_5 | ≈ 4.4 years | over the time budget |
| Bellman-Ford variants, 10 runs | all three on W_1, `textbook` on W_2 | 3 to 5 runs fitted in the 300 s budget | each run takes ~60 to 90 s (n − 1 full rounds) |

## 8. Reading the numbers

- **W_1 has a negative cycle**: 78 arcs, weight −235.30, and it reaches vertex
  100. Every distance asked about on W_1 is therefore −∞, by both algorithms.
  For the same reason, Bellman-Ford on W_1 cannot stop early, which explains
  its 88 s against 0.03 s on W_2, a graph of the same size.
- **The negative-cycle check takes about 10⁻⁷ s on W_2 to W_5.** It is not a
  timing error. Those graphs have no negative weight, so a graph without a
  negative arc cannot have a negative cycle, and the check returns in O(1).
  On W_1, it does real work (94 s).
- **`bellman_ford` `mean_s` includes the reversal; the variants' times do not.**
  Compare the variants with one another, not with the main row.
- **The Floyd-Warshall times on W_1 and W_2 come from 1 run each,** and the
  three harness processes of the first run were sharing the VM at the time.
  The `fewer_runs` status flags this.
