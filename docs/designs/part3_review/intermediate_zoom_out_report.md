# Goldfish Review Report: Stage 8, Intermediate Zoom Out

**Target:** `docs/designs/part3_design.md` (`FeatureRange 1-768`; the Detailed Implementation Plan from line 769 is known to lag and is reviewed only for scope regressions)

**Standard:** `zoom-out-standard.md`. **Mode:** REVIEW. **Baseline:** `part3_review/zoom_out_report.md` (Stage 5).

## Reviewer Status

| Reviewer | Status |
| --- | --- |
| `egm-zoom-out` (instance A) | completed |
| `egm-zoom-out` (instance B) | completed |

**Severity breakdown (deduplicated):** 0 Blocker, 3 Major, 9 Nit.

**Verdict (both instances agree):** approve after the Major trims, or once the owner explicitly accepts them. The core library shape is still simple: compile-time policies, free algorithm templates, no change to `concepts.hpp`, one reversal type, one whole-graph cycle check, and one in-place Floyd-Warshall. Most of the growth since Stage 5 is in measurement and operations (§7 implementation note, §8 `calibrate`, §10 harness), not in the library.

## Stage 5 findings: status

| Stage 5 | Topic | Status | Evidence (current lines) |
| --- | --- | --- | --- |
| M1 | Negative cycle blanks every Floyd-Warshall pair | Resolved | Per-pair `minus_infinity` and `on_cycle_` at 418, 430, rule at 463, proof at 470. Per-pair status at 523 and CUJ 2 (109). The cross-check compares statuses (528, 547). |
| M2 | Floyd-Warshall mean taken from a fit | Resolved | `fewer_runs` with 1 sample (88, 109, 524, 705). The fit is used only for VM approval and W_3 to W_5 (133, 735). |
| M3 | Biased calibration | Resolved, but it added scope | `on_pivot` hook (437-443), `progress` rows (507, 550), live ETA, Alternative X (731-735). The bandwidth measurement and kernel race were added on top; see Major 1. |
| M4 | Silent run, fixed timeouts | Resolved | Watchdog (550), progress rows at least every 60 s (495, 498), auto-shutdown re-armed before `start` (555), Risks row 749. Checkpointing correctly not adopted. |
| M5 | Inner loop hard to vectorize | Resolved, then overshot | Row pointers hoisted at 466, but two timed kernels were added; see Major 1. |
| M6 | Two booleans give three behaviours | Resolved in the FeatureRange | Enum at 287, Goal 3 reworded at 52. The plan regresses (855, 906, 919). |
| M7 | Three witnesses, one used | Resolved in the FeatureRange | Only witness is `negative_cycle_report::cycle` (365). `cycle_reaches_target` (280) and `negative_cycle()` (431) are booleans. The plan regresses (840, 927). |
| M8 | Harness re-specifies Part 2, gzip unexplained | Resolved | Decompress once (145, 541), imports Part 2 helpers (538), `fw_bytes_needed` follows `matrix_bytes_needed` (542). A separate `check_setup` (539) is justified. |
| M9 | Budget rule in the binary, tested in Python | Resolved in the FeatureRange | The rule is only in the binary (492), and the harness classifies sample counts only (545). The plan regresses (860, 941). |
| N1 | No mode runs the `mismatch` check | Resolved, with extra machinery | `bf_check` launches (494, 528, 544, 547); see Major 3. |
| N2 | Calibration stop rule unbounded | Resolved | "8 x last time fits" at 133 and 498. Reusing the `bf` pass became a flag; see Nit 3. |
| N3 | Implementation detail in Goals and feature sections | Partly resolved | Goal 8 (57) now points to §8 and §10, and `std::format` moved to the plan (845). New detail arrived at 42, 416, 466 and 550; see Nit 1. |
| N4 | Unused options on `find_negative_cycle` | Resolved | Signature at 380-383. |
| N5 | `fixed_point_weight` too generic | Open (owner decision, accepted) | 77, 202, 204-210: any floating `W`, run-time `decimals` 0..9. Low priority. |
| N6 | `azure_part3.sh` copies Part 2 | Open (accepted) | 552. Its auto-shutdown now differs from Part 2 (555 against `azure_part2.sh:79-82`), so leaving it as a copy is fine. |
| N7 | Two entry points plus `reversed_graph` | Open (owner decision B4), and it grew | 250-264, 302, 311, 325; see Nit 2. |
| N8 | No-copy row view | Open (owner decision, accepted) | 396-401, 421-433, 659. |
| N9 | Earlier detection in `find_negative_cycle` | Closed (not adopted) | 374-378. The right call for simplicity. |

**Totals:** 11 resolved (M5 and M7 counted resolved despite overshoot or plan regressions), 1 partly resolved (N3), 4 open by accepted owner decision (N5, N6, N7, N8), 1 closed by not adopting (N9). Three of the resolutions (M3, M5, N1) brought in the new scope behind Majors 1 and 3.

## Prioritized Findings (Blockers -> Major -> Nits)

### Major 1: `calibrate` and Floyd-Warshall have become a performance lab - Severity: Major
* **Reviewer Lens:** `egm-zoom-out` (A: Z1; B: New-1, New-4)
* **Affected Scope:** Lines 42, 466, 498, 505-506, Alternative Y 737-741
* **Discrepancy Description**: The fixes for M3 and M5 added three things the assignment does not need:
  * a branchless and a branchy Floyd-Warshall kernel, raced on the VM at n = 4,000 before one is chosen;
  * a plain-loop reference that both kernels must match bit for bit, with its own differential test file;
  * a 1 GB memory-bandwidth measurement with a `fit,bandwidth_bytes_per_s` model.

  Nothing downstream uses the bandwidth number. The estimates (513, 546) and the VM approval use the c·n³ fit and the progress ETA. Because of the race, the kernel that produces the answers is chosen at run time on the VM, so the tested build is not fixed in advance. The section also contradicts itself in two places:
  * line 466 emits `calib,floyd_warshall_branchless,…` and `calib,floyd_warshall_branchy,…`, but the row table at 505 has only `calib,floyd_warshall,<n>,…`;
  * Alternative Y rejects tiling partly as "more code to test", which applies equally to a second kernel.
* **Rationale & Recommendation**:
  * Keep one kernel, chosen now: hoisted `dist(i, k)`, row pointers, contiguous j loop. Instance B suggests the branchy `if`, because it matches the algorithm taught in class.
  * Check vectorization once, locally, and record the result as a note for the report.
  * Drop the A/B `calib` rows, the bandwidth measurement and the `bandwidth_bytes_per_s` model. Keep the bandwidth argument as prose in The Problem (42).
  * Move what remains of 466 into the Detailed Implementation Plan.
  * If the owner wants the kernel comparison anyway, make it a one-off local experiment reported in the method section, not a VM step.

### Major 2: Streaming and restart machinery is under-specified and heavier than needed - Severity: Major
* **Reviewer Lens:** `egm-zoom-out` (A: Z2; B: New-2)
* **Affected Scope:** Lines 57(d), 499, 543-551
* **Discrepancy Description**: Every streamed row is "tagged with an attempt id", and on restart the rows of incomplete attempts are discarded. The CSV schemas at 551 have no attempt column, so the tag has nowhere to live. Rows go into the shared result CSVs only so progress is visible, but progress already has its own channels (console ETA, `azure_part3.sh status`). For about one run of 5 graphs, the tag-and-discard step is the part most likely to be buggy. Both instances also call the watchdog's "5 x longest gap" term redundant, since every long mode prints a row at least every 60 s (498, 550).
* **Rationale & Recommendation**:
  * Write each launch's stdout to its own file, for example `runs/<graph>_<mode>[_<target>].csv`.
  * A launch counts as complete when its file ends with a `done` row. Otherwise rerun it and overwrite the file.
  * Build `results_part3.csv` and `paths_part3.csv` from the complete files only. Partial rows then never reach the statistics, attempt ids and discard-on-restart disappear, and restart comes for free.
  * State the watchdog as one fixed silence limit, for example "no row for 2 x `--budget-seconds`".

### Major 3: Run-time self-checks duplicate the test suite - Severity: Major (cumulative)
* **Reviewer Lens:** `egm-zoom-out` (A: Z3, rated Major; B: New-3, rated Nit; kept at the higher rating)
* **Affected Scope:** Lines 494, 496, 510-511, 528-530, 545, 547
* **Discrepancy Description**: There are three layers of run-time verification on top of the differential tests:
  * `check,variants_agree` in the binary, which the test plan already covers;
  * `check,witness`, a re-verified witness and its weight, with an `error` status;
  * `fw_vs_bf` in the harness, which adds five `bf_check` launches per graph, a separate launch-key type, an exclusion rule for statistics, and a `mismatch` status.

  Each one is small. Together they add a row kind, two statuses, a metric, a launch-key type and harness rules.
* **Rationale & Recommendation**: Keep one run-time cross-check on real data and rely on the tests for the rest. Both instances agree that `fw_vs_bf` has the most value: it costs five cheap launches and catches faults specific to the environment.
  * Drop `variants_agree`: rely on the tests, or compare W_1 and W_2 distances in the harness.
  * The instances split on the witness:
    * A: turn the witness check into an assert in the checked build, which removes the `check` row kind;
    * B: keep `witness`, because it is the evidence that gets printed.

  Owner's call.

### Nit 1: Altitude, with implementation arithmetic in The Problem and the Technical Plan - Severity: Nit
* **Reviewer Lens:** `egm-zoom-out` (A: N-a; B: N3 status)
* **Affected Scope:** Lines 42, 416, 466, 550
* **Discrepancy Description**: The rest of N3 from Stage 5:
  * The Problem carries a bandwidth derivation (bytes per pivot, TB per run, GB/s);
  * §7 carries `std::size_t`, pointer hoisting, compiler flags and mask details;
  * §10 carries the watchdog formula.

  Instance B calls line 466 the worst weeds paragraph in the FeatureRange.
* **Rationale & Recommendation**: In The Problem, keep "estimated 5 to 10 h, basis in §7". Move the arithmetic and the loop notes to the plan.

### Nit 2: The reversal type-safety machinery grew in round 3 - Severity: Nit
* **Reviewer Lens:** `egm-zoom-out` (A: N-b; B: N7 status, New-6)
* **Affected Scope:** Lines 250-264, 302, 311, 325
* **Discrepancy Description**: On top of `reversed_graph`, the design now has:
  * an `is_reversed_graph` trait;
  * a constrained friend factory;
  * the `reverse_input` concept;
  * a `!is_reversed_graph` constraint on `bellman_ford`;
  * four or five negative compile tests.

  All of this guards one call site in one program. It is compile-time only and cheap, and the `bellman_ford(reversed(g), t)` guard prevents a real silent error, so both instances accept it as decided. It is still the largest piece of speculative safety in the library.
* **Rationale & Recommendation**: Leave it as decided. If it is revisited, the cheapest cut is to drop the convenience `bellman_ford` overload and its constraint, since the program always calls `bellman_ford_on_reversed` (494). Instance A would also drop the `reversed(reversed(g))` rejection and its trait.

### Nit 3: `--no-bf-pass` flag and harness rule to save one Bellman-Ford pass - Severity: Nit
* **Reviewer Lens:** `egm-zoom-out` (A: N-c; B: New-5)
* **Affected Scope:** Lines 487, 498, 548
* **Discrepancy Description**: A CLI flag plus harness state ("a `bf` answer run is already recorded") saves one pass of at most about 100 s. Calibration runs before the full run, so the condition fires only when calibration is rerun.
* **Rationale & Recommendation**: Always run the pass and drop the flag and the rule. Alternatively, have `--calibrate-only` run the `bf` step, which is needed anyway, and remove the pass from `calibrate`.

### Nit 4: A second way to read available memory - Severity: Nit
* **Reviewer Lens:** `egm-zoom-out` (A: N-d)
* **Affected Scope:** Line 527
* **Discrepancy Description**: "`MemAvailable` from /proc/meminfo on Linux, psutil elsewhere" duplicates `psutil.virtual_memory().available`, which already reads that value on Linux. Part 2 already uses psutil with a 0.9 factor (`analysis/harness_weighted.py:308`).
* **Rationale & Recommendation**: Use psutil everywhere with a single margin rule, ideally Part 2's.

### Nit 5: The `out_of_range` status cannot fire - Severity: Nit
* **Reviewer Lens:** `egm-zoom-out` (A: N-e)
* **Affected Scope:** Line 529
* **Discrepancy Description**: The vertex ids come from the config (10..100), and every graph has n >= 25,000.
* **Rationale & Recommendation**: Have the program reject a bad id with `error` and an exit code, and drop the status.

### Nit 6: The all-pairs full-measurement branch is the rare case, but CUJ 4 does not say so - Severity: Nit
* **Reviewer Lens:** `egm-zoom-out` (A: N-f)
* **Affected Scope:** Lines 121, 497
* **Discrepancy Description**: At n = 25,000 the `ok` branch fires only if one pass takes under about 24 ms on a 600 s budget, which is plausible on W_1 at best. It is an owner decision and cheap.
* **Rationale & Recommendation**: State the threshold in CUJ 4 so the reader knows the `ok` branch is the exception.

### Nit 7: `skipped` rows for the matrix storage, which Part 3 never runs - Severity: Nit
* **Reviewer Lens:** `egm-zoom-out` (B: New-7)
* **Affected Scope:** Lines 63, 490, 517, 526
* **Discrepancy Description**: Matrix storage is a non-goal, yet the harness emits `skipped` rows for it.
* **Rationale & Recommendation**: State the non-goal once in the report's method section and emit no rows. `skipped` then means only "turned off in config".

### Nit 8: The on-graph optimization comparison is still in scope - Severity: Nit
* **Reviewer Lens:** `egm-zoom-out` (B: R1)
* **Affected Scope:** Lines 86, 484, 494, 536, 568
* **Discrepancy Description**: The comparison costs a CLI option, per-variant time and `info` rows, a config column, metrics and a `check` row. Case study 3 asks for each algorithm's time, not a comparison of variants. Stage 5 M6 named it the first cut.
* **Rationale & Recommendation**: Keep it, since the owner decided (86). List it explicitly as the first thing to drop if VM time or the schedule gets short.

### Nit 9: The output protocol now rivals the library sections in size - Severity: Nit
* **Reviewer Lens:** `egm-zoom-out` (A: N-g; B: R2)
* **Affected Scope:** §§8-10 (474-570); Decision log 87-89
* **Discrepancy Description**: The protocol tables (9 row kinds, 10 statuses, 10 metrics) take about as much space as §§1-7 together. The Decision log now summarizes three review batches, so a reader must open three reports to see why items like the watchdog floor exist.
* **Rationale & Recommendation**: Majors 2 and 3 would cut about a third of the tables. The tables are otherwise a legitimate interface contract. Optionally, cite the implementing section for each Decision log item.

## Plan items (from line 769) that restore removed scope

These are not mismatch findings. They are listed because they undo Stage 5 resolutions; the FeatureRange should win in the Stage 9 revision.

| Plan lines | Regression |
| --- | --- |
| 840, 899, 903, 927 | Bring back Floyd-Warshall and Bellman-Ford negative-cycle witnesses (undoes M7) |
| 855, 906, 919, 965 | Bring back four optimization combinations and `frontier_only` (undoes M6) |
| 860 | "launch ... with a timeout" (undoes M4) |
| 941 | Python test of the budget rule (undoes M9) |
| 865 | VM default 32 GB, against 16 GB at line 553 |
| 880, 909 | Test `float` for `fixed_point_weight` (widens N5) |
| 965 | Marks all-pairs as `extrapolated` everywhere, against CUJ 4 and line 88, which allow `ok` on W_1 and W_2 |
