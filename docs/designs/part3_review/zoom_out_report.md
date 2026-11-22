# Stage 5: Initial Zoom Out Review

* **Target:** `docs/designs/part3_design.md` (lines 1..825; FeatureRange 1..628, DetailedRange 629..825)
* **Date:** 2026-11-21
* **Reviewers:** egm-zoom-out (with `zoom-out-standard.md`), egm-inventive-catalyst, run in parallel through egm-synthesizer
* **Context given:** `docs/designs/part3.md`, `AGENTS.md`, `docs/CONVENTIONS.md`, `include/graph/concepts.hpp`, `io/edge_list.hpp`, `algorithms/path.hpp`, `storage/csr.hpp`, `app/benchmark_weighted.cpp`, `analysis/harness_weighted.py`, `analysis/azure_part2.sh`
* **Result:** 0 blockers, 9 major findings, 9 nits after merging duplicates. Verdict from egm-zoom-out: approve once the major findings are fixed.

Both reviewers accept the overall shape: compile-time policies, free algorithm templates, no change to `concepts.hpp`. The inventive catalyst found no hidden compromise in `reversed_graph`, the implicit virtual source, the BFS marking of −∞, the parser hook or the fixed-point weights. Most findings are about how Floyd-Warshall runs, is measured and reports, plus trimming.

Spot checks by the author against the design and the code: M1 (line 401, "every distance and predecessor except the witness is unspecified"), M4 (line 455, "one timeout per launch"), M6 (line 287, the stop condition `(stop_when_stable or frontier_only)`) and M8 (`read_edge_list` opens a plain `std::ifstream`; `harness_weighted.py:122` has `plain_text` for gzipped inputs) are confirmed. The other line numbers are the reviewers'.

## Major findings

### M1. A negative cycle blanks every Floyd-Warshall pair [catalyst; the nearest to a blocker]

* **Where:** Technical Plan §7 (line 401), Goal 6, CUJ 2, Alternative H, Acceptance (line 825).
* **Problem:** whether directed W_1 has a negative cycle is unknown. If it has one, all 20 pairs of case study 2 become "unspecified"; §8 names no status for them; the Bellman-Ford cross-check would then report `mismatch`, which the Acceptance line forbids. It is the "one flag blanks every row" outcome that Alternative H rejected for Bellman-Ford.
* **Fix:** after the main loop, collect the vertices t with dist(t, t) < 0. A pair (i, j) is −∞ exactly when some such t has dist(i, t) < +inf and dist(t, j) < +inf (reachability stays correct even with a cycle); every other pair, and its predecessor chain, is correct. Add `all_pairs::undefined(i, j)` (O(number of such t) per pair, used only for requested pairs); print `ok` / `unreachable` / `negative_cycle` per pair; keep or relax the `tree_from` assertion to "the row has no undefined cell"; reuse the Bellman-Ford "cycle reaching the target, partly" test pair by pair; make the cross-check compare statuses as well as values; state that undefined cells are only "not +inf", since their values may be very large.

### M2. The 10-run Floyd-Warshall mean is a fit although a full-size run exists [zoom-out, catalyst]

* **Where:** Goal 8 (line 47), CUJ 2 (line 79), §8 `fw` (line 421), the `estimate` row.
* **Problem:** the single run at n = 25,000 is measured, yet the reported mean comes from a c·n³ fit on random graphs of n ≤ about 8,000. Floyd-Warshall time is nearly deterministic and depends on reachability (row skipping), so the real run is the better number. Goal 8 has no status for exactly one sample (`ok` = 10 runs, `fewer_runs` = 2 to 9).
* **Fix:** report the measured run with samples = 1 and status `single_run` (or widen `fewer_runs` to 1 to 9). Use the fit only to approve VM time and for graphs not run (W_3 to W_5). If `estimate` rows stay, add a source column (`measured` or `fit`).

### M3. The calibration fit is biased, so the approved VM time is unreliable [catalyst; related to N2]

* **Where:** CUJ 6, §8 `calibrate` (line 424), Goal 8, the VM-time open question (line 65), the Azure `calibrate` step (line 725).
* **Problem:** at small n the matrices sit in cache; at 7.5 GB the loop is limited by memory bandwidth, so the fit underestimates. Random digraphs are close to strongly connected and do not predict how many rows W_1 and W_2 skip.
* **Fix:** an optional no-op observer `on_pivot(k, rows_not_skipped)` on `floyd_warshall`; `benchmark_part3 fw` prints a flushed `progress,floyd_warshall,<k>,<elapsed_micros>,<rows_done>` row every K pivots. The real answer run then calibrates itself: after about 600 s it shows an ETA (a lower bound early on), and the owner lets it continue or stops it. The random-graph fit remains only for graphs not run. Cost: `progress` in `parse_row`, in the row table and in the tests (the harness rejects unknown row kinds), and a test that the observer is called n times.

### M4. A multi-hour run is silent and guarded by fixed timeouts [catalyst; depends on M3]

* **Where:** §10 "one timeout per launch" (line 455), Goal 8 ("a run that dies keeps what was measured"), CUJ 6 auto-shutdown, §8 `fw`.
* **Problem:** the harness timeout and the VM auto-shutdown are sized from an uncertain estimate: too short and a healthy run is killed after hours with nothing kept; too long and a hung run burns credit. Resume does not help: an incomplete `fw` step restarts at k = 1.
* **Fix:** a watchdog on `progress` rows (kill only when none arrived for N times the expected interval); live ETA on stderr and in `azure_part3.sh status`, so the owner can push the auto-shutdown back; a test with a fake slow binary. Optional: checkpoint dist and pred every few hours (about 8 GB, fits the 64 GB disk), only if the measured ETA is above about 4 h.

### M5. The Floyd-Warshall inner loop is written in a form that is hard to vectorize [catalyst]

* **Where:** §7 algorithm (lines 392 to 397), Alternative K, the time estimate in "The Problem" (line 34).
* **Problem:** every cell goes through an index function, and two arrays of different widths are written behind a branch. The constant factor of this loop is most of the plan's VM cost.
* **Fix:** hoist `dik` and the row pointers (rows i and k of dist and pred), write the j loop without a branch over contiguous rows (results bit-identical); check vectorization once with `-Rpass=loop-vectorize` (clang) or `-fopt-info-vec` (GCC) and measure the gain; say in the report that the loop is tuned. Owner's call: running the i loop on the VM's 4 vCPUs could give up to about 4×, but needs a threading dependency and makes the comparison with single-threaded Bellman-Ford unfair unless Bellman-Ford is parallelized too.

### M6. Two Bellman-Ford booleans give only three behaviours [zoom-out]

* **Where:** §4 (lines 241 to 245, 287), Goal 3 (line 42), §8 `--optimizations` (lines 413, 420), config `bf_optimizations` (line 715), Acceptance (line 825), the four-combination test (line 779).
* **Problem:** the stop condition is `changed_this_round is empty and (stop_when_stable or frontier_only)`, so "frontier" and "both" are the same run. Goal 3's "each can be switched off and does less work" is false for `stop_when_stable` when `frontier_only` is on, and the four-combination test is partly empty.
* **Fix:** one `enum class bellman_ford_variant { textbook, stop_when_stable, frontier }`; three variants in config and acceptance; reword Goal 3. The on-graph comparison is not asked by the case studies and is the first thing to cut if time is short.

### M7. Three negative-cycle witnesses, one used [zoom-out]

* **Where:** `target_paths::cycle` (lines 236 to 238), self-loop witness rule 3 (line 298), `all_pairs::cycle_` and `cycle()` (lines 360, 375), the witness walks (lines 398, 700), their tests (lines 759, 763, 787).
* **Problem:** only the `find_negative_cycle` witness is printed (lines 327, 433); the assignment asks only whether a negative cycle exists.
* **Fix:** keep `cycle_reaches_target` and `negative_cycle()` as booleans; drop `target_paths::cycle`, `all_pairs::cycle_` and `cycle()`; one `detail` pointer-walk helper for the single witness in `negative_cycle_report`.

### M8. The Part 3 harness re-specifies `harness_weighted.py` and never says how gzipped inputs are read [zoom-out]

* **Where:** §10 (line 455), §13 (line 720), the data-flow diagram (lines 111 to 118).
* **Problem:** inputs are `data/part3/grafo_W_k.txt.gz`, but `read_edge_list` opens a plain `std::ifstream`. `harness_weighted.py` already has `find_input`, `plain_text` (temporary decompression), `vertex_count`, `select_rows`, `check_setup` and `append_csv`.
* **Fix:** import those helpers (importing does not modify Part 2, so the non-goal holds); add "decompress once per graph" to the data flow; shape `fw_bytes_needed` like `matrix_bytes_needed`.

### M9. The repetition-budget rule lives in the binary but is tested in Python [zoom-out]

* **Where:** §8 `bf` / `fw` (lines 420, 421) against `test_harness_part3.py` (line 801, "100 s each stops after 6 runs").
* **Problem:** the harness only sees the rows the binary prints; the Python test can pass only if the harness re-implements the rule, and two copies can drift.
* **Fix:** the rule lives only in the binary and is tested end to end in `make test-part3` (the fixture with a tiny `--budget-seconds` prints fewer `time` rows than `--runs`); the harness test only checks how sample counts are classified.

## Nits

* **N1. No mode runs the `mismatch` cross-check** [zoom-out; see M1]. Lines 445, 610, 421. Either `fw` also runs Bellman-Ford toward the five set vertices (cheap on W_1 and W_2), or drop the run-time check and the `mismatch` status on path rows, since the differential tests (lines 777, 796) already cover agreement.
* **N2. The calibration stop rule is not bounded** [zoom-out; related to M3]. CUJ 6 (line 103), §8 `calibrate` (line 424). Doubling n multiplies the time by about 8, so the last size can take up to about 8 × 600 s. Run the next size only if 8 × the last time fits the budget. Reuse the first `bf` answer run instead of a separate Bellman-Ford calibration pass.
* **N3. Goals and feature sections carry implementation detail** [zoom-out]. Goal 8 (line 47) is a whole measurement protocol; lines 418, 357 and 64 name `std::format("{:.2f}")`, `std::size_t` and `round(w*100)`. Shorten Goal 8 to its outcome, move the protocol to §8 and the format and index details to the Detailed Implementation Plan.
* **N4. `find_negative_cycle` takes options no caller changes** [zoom-out]. Line 324. Drop the parameter, keep `stats`.
* **N5. `fixed_point_weight` is more generic than its one user** [zoom-out]. Lines 168 to 174, test entry 17 (line 740). Fix `decimals` as a constant or template parameter and allow only `double`; the `float` test and the 0..9 assertion go away.
* **N6. `azure_part3.sh` copies the whole Part 2 script** [zoom-out]. §10 (line 457), §14 (line 725). A copy is acceptable; cheaper would be `HARNESS`, `INPUTS`, `RESULTS`, `SETUP_TARGETS` variables and a `calibrate` step in `azure_part2.sh`.
* **N7. Two entry points and `reversed_graph` save one O(n + m) reversal** [zoom-out]. §§3 and 4 (lines 185 to 272). The owner decided this (B4); acceptable. If ever revisited: one public `bellman_ford(g, t)`, with the reversal in `detail`.
* **N8. The no-copy row view costs more than copying 5 rows (about 200 KB)** [zoom-out]. §§6 and 7, Alternative L. Owner's decision; keep it. It is the largest optional simplification left; keep the bounded `reconstruct_path` either way.
* **N9. Earlier detection in `find_negative_cycle`** [catalyst]. §5 (lines 314 to 319). Check the predecessor graph for a cycle at doubling intervals, stop at the first one and use it as the witness; behind an option, with the stop round in stats; test with the ring through all n vertices. Not in `bellman_ford_on_reversed`, where exact −∞ marking needs round n.

## Suggested order

1. M1 with N1: decides whether case study 2 has any answers on W_1 and whether the Acceptance line can hold.
2. M2, M3, M4 and N2 together: how Floyd-Warshall is measured and run on the VM.
3. M6, M7, M8, M9: independent, cheap trims.
4. M5: depends on how much tuning the owner wants.
