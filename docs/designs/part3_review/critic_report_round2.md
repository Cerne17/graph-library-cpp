# Goldfish Review Report: Stage 7 Critic Review, round 2

**Target:** `docs/designs/part3_design.md` (lines 1-699, FeatureRange; the Detailed Implementation Plan from line 700 is out of scope)
**Mode:** REVIEW
**Standards:** egm-design and egm-structural used `resources/document_standards/feature-spec-standard.md`; egm-style used its persona's rules only.

## Reviewer Status

| Reviewer | Status | Raw findings |
| --- | --- | --- |
| `egm-design` | completed | 12 (5 Major, 7 Nit) |
| `egm-structural` | completed | 10 (2 Major, 8 Nit) |
| `egm-style` | completed | 15 (3 Major, 12 Nit) |

After deduplication there are **0 Blockers, 9 Major and 21 Nit** (30 findings). Seven raw findings were merged into others; each merged item lists its source ids. Ids use the form `<reviewer>-2-<n>` and keep each reviewer's own numbering. Structural's M1, M2 and N1-N8 map to `structural-2-1` to `structural-2-10`.

**Re-raised settled findings:** none. All three reviewers confirmed that they raised none.

All reviewers found every section the feature-spec standard requires: problem, goals, non-goals, architecture, alternatives with reasons, and risks. None of the findings is a missing section.

---

## Prioritized Findings (Blockers -> Major -> Nits)

### Blockers

None.

### Major

#### design-2-1: Goal 3 claims a work ordering the variants do not have - Severity: Major
* **Reviewer Lens:** `egm-design` (merged: `style-2-2`)
* **Affected Scope:** Lines 50, 307-325
* **Discrepancy Description:** Goal 3 says the tests will show, through round and relaxation counters, that "each later variant does no more work than the one before". With the in-place pseudocode this is false for `frontier` against `stop_when_stable`:
  * `stop_when_stable` scans every finite vertex in id order, so an improvement can spread many hops within one round.
  * `frontier` only scans the vertices that changed in the previous round.
  * Counterexample for rounds: the arcs t->1->2->...->n settle in one round under `stop_when_stable` but take n rounds under `frontier`.
  * Counterexample for relaxations: a high-degree vertex improved once per hop costs about k·D relaxations under `frontier` and about 2(D+2k) under `stop_when_stable`.
  * The style lens found a second gap: the heading says "shown to prune", but the acceptance test only checks "no more work" (≤). Three variants that do equal work would pass it.
* **Rationale & Recommendation:** As written, the test can only pass on hand-picked graphs, and the report's comparison would rest on a claim that is false in general. Pick one fix:
  1. Make rounds synchronous (double-buffered `dist`) in all variants. Then `frontier` runs the same rounds as `stop_when_stable` over a subset of its arcs, and the ordering holds. Add a "strictly fewer on fixture X" check so the goal shows pruning.
  2. Weaken Goal 3 to "textbook ≥ stop_when_stable in both counters; frontier measured and reported, not ordered", and rename the heading to match.

  Line 307 ("can only shorten the number of rounds") holds only within one variant. Fix it either way.

#### design-2-2: Floyd-Warshall run time is likely underestimated, and the branchless loop may make it worse - Severity: Major
* **Reviewer Lens:** `egm-design` (merged: `style-2-4`)
* **Affected Scope:** Lines 42, 84, 98, 440, 442, 641, 647; Alternatives J-L
* **Discrepancy Description:**
  * At n = 25,000 the 7.5 GB matrices are far larger than any cache. Each pivot streams about 24·n² bytes, about 15 GB, so a graph moves about 375 TB in total.
  * At 10-20 GB/s single-core bandwidth that is about 5-10 h per graph. The stated 1.5 h lower bound would need about 70 GB/s.
  * The masked select on line 440 writes every cell on every pivot, so every cache line is dirtied. A branchy update that is rarely taken late in the run could roughly halve the traffic.
  * No alternative considers a tiled (blocked) in-place Floyd-Warshall. It keeps the single-matrix memory optimization and cuts memory traffic sharply.
  * Style lens: the "1.5 to 8 h" range (and the 5-20 s and 20-100 s figures) gives no assumed throughput, and the same numbers appear on lines 42, 84, 98 and 641.
* **Rationale & Recommendation:** The VM budget, the 22 h split and the watchdog/ETA all depend on this estimate. The calibration sizes (up to about 192 MB) do not capture behaviour at 7.5 GB. Recommendations:
  * Add a bandwidth-based bound (bytes per pivot × n ÷ measured STREAM bandwidth) next to the c·n³ fit, and widen the range.
  * Benchmark the branchless and branchy j loops at a size larger than cache before choosing one.
  * Add tiled Floyd-Warshall as an alternative with an explicit accept or reject reason. Rejecting it to keep "the class algorithm" is acceptable if stated.
  * State the throughput assumption once (line 42) and refer to it elsewhere.

#### design-2-3: The `tree_from` precondition conflicts with how the program uses it - Severity: Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Lines 98, 409, 442
* **Discrepancy Description:** `tree_from(i)` requires a row with no `minus_infinity` cell, asserted in debug. The program calls it for printed rows and `ok` pairs only. On W_1 with a negative cycle, row i can have `minus_infinity` cells to other vertices while (i, j) is `ok`, so the assertion fires on a valid request. The whole-row check also costs O(n·|on_cycle|).
* **Rationale & Recommendation:** Debug runs and the CUJ 2 tests on graphs with negative cycles would abort. The argument on line 442 only needs the requested cell to be unaffected, because every vertex on its pred chain is then unaffected too. Make the precondition per target: `reconstruct_path(tree_from(i), j)` is valid when `!minus_infinity(i, j)`. Drop the whole-row assertion, or replace it with a check on that one cell.

#### design-2-4: Goal 8(d), "loses at most its unfinished step", contradicts the restart unit in §10 - Severity: Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Lines 55 (8d), 162, 463-464, 504, 680
* **Discrepancy Description:** A (graph, mode) launch counts as complete only when its `done` row arrives, and the harness discards incomplete attempts. A `fw` launch runs the hours-long answer run, then the path rows, then five Bellman-Ford cross-checks. A crash during the cross-checks throws away the finished Floyd-Warshall run. `bf --optimizations` has the same problem across its steps.
* **Rationale & Recommendation:** The real unit of loss is the launch, not the step, so Goal 8(d) and the VM-time risk row understate it. Pick one fix:
  1. Keep the `time` and `path` rows of an incomplete `fw` attempt once a check-phase marker row has been seen.
  2. Move the cross-check into its own launch that reads the `path` rows already written.
  3. Restate Goal 8(d) as "at most its unfinished launch" and record that cost in the risk table.

#### design-2-5: `check_setup` from `harness_weighted.py` cannot be reused as-is - Severity: Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Line 504
* **Discrepancy Description:** `harness_weighted.check_setup` (harness_weighted.py:153-169) is specific to Part 2:
  * it requires `data/baseline.txt`;
  * its error message points to `make benchmark-weighted`;
  * it reads `cfg["labels"]`.

  `config_part3.csv` has no `labels` column, so the call raises `KeyError`, and `data/part3/` has no `baseline.txt`.
* **Rationale & Recommendation:** The design says that importing the Part 2 helpers is enough and that Part 2 stays unchanged. That holds for `find_input`, `plain_text`, `vertex_count`, `select_rows` and `append_csv`, but not for `check_setup`. Remove it from the import list and write a Part 3 `check_setup` in `harness_part3.py`.

#### structural-2-1: The binary's output contract has no row kind for calibration results or for the timed all-pairs target - Severity: Major
* **Reviewer Lens:** `egm-structural`
* **Affected Scope:** Lines 122, 467, 470-480
* **Discrepancy Description:** `calibrate` prints Floyd-Warshall times for several random-graph sizes plus a c·n³ fit, but the row-kind table has no row for them. A `time` row (`time,<algorithm>,<run>,<micros>`) has no column for n, out-degree, the fitted c, or the sampled `allpairs` target. Line 480 says the harness combines "the calibration output" with step rows, but that output has no defined format.
* **Rationale & Recommendation:** §8 is the interface that both the C++ binary and the Python harness are written from, so a gap here means each side guesses. Add row kinds, for example `calib,<n>,<arcs>,<run>,<micros>` and `fit,floyd_warshall,<c>,<points>`. Alternatively, give `time` rows an optional parameter column and list which modes emit it.

#### structural-2-2: The `results_part3.csv` schema does not say how estimates, cycle witnesses and spread are stored - Severity: Major
* **Reviewer Lens:** `egm-structural`
* **Affected Scope:** Line 506 (against lines 92, 104, 110, 492, 504)
* **Discrepancy Description:** The schema is `graph,mode,algorithm,metric,value,samples,status,note`. The CUJs and §9 promise that the file also holds:
  * the cycle witness and its weight;
  * the memory need and the c·n³ estimate;
  * the pass-time spread, the number of targets and the formula;
  * "estimate rows".

  There is no list of `metric` names, so nothing says which fields carry these values.
* **Rationale & Recommendation:** `report_part3.py` and the restart logic both read this file, so the four case-study tables need a fixed vocabulary. Add a table of `metric` names per mode below line 506, for example `mean_s`, `median_s`, `estimate_s` (formula in `note`), `memory_bytes`, `negative_cycle` (witness and weight in `note`), `pass_spread_s` and `targets_measured`. Also state that "estimate rows" means rows with `status=extrapolated`.

#### style-2-1: "Far over the step budget on every case-study graph" is not backed for W_1 and W_2 - Severity: Major
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** Lines 42, 110, 363, 641
* **Discrepancy Description:** Line 110 says one all-pairs Bellman-Ford repetition is "far over the step budget on every case-study graph", and line 641 says it would take "days or more". Line 42 is narrower ("on the larger graphs ... days to months"). The design gives no per-pass time for W_1 or W_2. At n = 25,000, a pass of about 20 ms gives about 500 s, which is under the 600 s budget.
* **Rationale & Recommendation:** This claim decides that all-pairs is always reported as `extrapolated`, so it needs a basis. Either state the assumed per-pass time and the n × pass total for each graph, or reword it as "estimated over budget on W_3-W_5; on W_1 and W_2 the budget rule decides". Make lines 42, 110 and 641 agree.

#### style-2-3: The 32 GB VM is called "the smallest size that fits" without justification - Severity: Major
* **Reviewer Lens:** `egm-style` (merged: VM-size part of `design-2-12`)
* **Affected Scope:** Lines 55 (8c), 96, 442, 507, 685, 694
* **Discrepancy Description:** The stated peak needs are 7.5 GB of Floyd-Warshall matrices plus a CSR of a few MB, and a 640 MB temporary for the W_5 reversal. On those numbers a 16 GB VM fits, so "smallest size that fits (32 GB)" is unsupported.
* **Rationale & Recommendation:** The owner approves cost against this sizing. Either state the peak resident estimate that rules out 16 GB (OS, page cache, decompressed W_5 text, harness process) or call 32 GB "the chosen size, with X GB headroom". Otherwise, default to 16 GB.

### Nit

#### design-2-6: The weight-magnitude bound is assumed, not checked - Severity: Nit
* **Reviewer Lens:** `egm-design` (merged: `style-2-15`)
* **Affected Scope:** Line 204
* **Discrepancy Description:** The exactness argument rests on "|scaled w| ≤ 1,000 on the case-study graphs", which has no source and is not enforced.
* **Rationale & Recommendation:** Cite how it was found (for example, the max |w| from a scan of `data/part3/`). Better, have the program emit an `info,max_abs_weight` row and report an error if (n − 1) × max |w| > 2^53.

#### design-2-7: Overflow and NaN safety of Floyd-Warshall under negative cycles is not argued - Severity: Nit
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Lines 204, 431-437, 440
* **Discrepancy Description:** Affected cells can overflow to −inf, and then −inf + +inf = NaN can be computed. The result is still safe: NaN < x is false, so NaN is never stored, and `minus_infinity` stays correct. The design only says that values "can grow past the bound".
* **Rationale & Recommendation:** State this argument in §7, and add a test where a negative cycle drives values to overflow.

#### design-2-8: "Bit-identical" is unqualified, and hoisting with i == k aliases rows - Severity: Nit
* **Reviewer Lens:** `egm-design` (merged: `style-2-9`)
* **Affected Scope:** Line 440
* **Discrepancy Description:** Hoisting dist(i, k) gives identical results only when dist(k, k) ≥ 0. When i == k the two row pointers alias, so `__restrict` would be undefined behaviour. The style lens adds that the "measured speed-up" has no baseline, and that "bit-identical" is stated as a fact but no test checks it.
* **Rationale & Recommendation:** Write "bit-identical on cells not affected by a negative cycle". Skip row k when dist(k, k) ≥ 0, which makes a restrict-qualified loop legal. Name the baseline and graph size of the speed-up, and name the test that compares the two loops bit for bit.

#### design-2-9: The cost of the `fw_vs_bf` column check is not stated - Severity: Nit
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Lines 98, 407, 464
* **Discrepancy Description:** Classifying column t with `minus_infinity` costs n × |on_cycle| strided reads. If W_1 has a large negative SCC, that is up to about 3×10⁹ cache-missing reads over the five targets. This time is not measured, but it still uses VM time.
* **Rationale & Recommendation:** Precompute once a per-row "reaches on_cycle" bitmap and a per-column "reached from on_cycle" bitmap, which makes `minus_infinity` O(1).

#### design-2-10: It is unclear who skips the calibration's Bellman-Ford pass - Severity: Nit
* **Reviewer Lens:** `egm-design` (merged: `style-2-13`)
* **Affected Scope:** Line 467
* **Discrepancy Description:** "Skipped when a `bf` answer run is already recorded" reads as if the binary decides, but the binary cannot see the results CSV.
* **Rationale & Recommendation:** Make it a harness decision passed as a flag (for example `--no-bf-pass`), and write "the harness omits it when ...".

#### design-2-11: The CUJ 4 fallback contradicts the budget rule - Severity: Nit
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** Line 110 (against lines 461 and 466)
* **Discrepancy Description:** The first run of a step always runs, so `allpairs` always times at least one pass. "If one pass alone exceeds the budget, the per-pass time is the single case-study pass" describes a case that cannot happen.
* **Rationale & Recommendation:** Drop the fallback, or say the first `allpairs` pass is reported. (Missing `--k`/`--seed` defaults: see structural-2-5.)

#### design-2-12: How the `no_memory` rule measures "the VM's memory" is not stated - Severity: Nit
* **Reviewer Lens:** `egm-design` (its `ok`-status and VM-size parts are merged into structural-2-6 and style-2-3)
* **Affected Scope:** Line 494
* **Discrepancy Description:** The `no_memory` status compares the estimated need against "the VM's memory", but the design does not say how that memory is measured.
* **Rationale & Recommendation:** Specify `MemAvailable` from `/proc/meminfo` and a safety margin.

#### structural-2-3: Build targets and harness options are used but not defined in scope - Severity: Nit
* **Reviewer Lens:** `egm-structural` (merged: the `--only` part of `style-2-12`)
* **Affected Scope:** Lines 54, 507, 510-514
* **Discrepancy Description:** `make benchmark-part3`, `make test-part3`, the harness option `--only` and its `calibrate` step all appear in §10. The Makefile has none of them yet, §11 Housekeeping lists no Makefile change, and only the out-of-scope plan defines them. Because `make test-part3` is not part of `make test`, Goal 7 (line 54) leaves out the harness tests.
* **Rationale & Recommendation:** Add a Makefile bullet to §11 that names the two targets and says `make test` is unchanged. Add one line to §10 that gives the harness CLI (`--only <graph>`, `--calibrate-only`).

#### structural-2-4: Variant names differ between the CLI and the enum - Severity: Nit
* **Reviewer Lens:** `egm-structural`
* **Affected Scope:** Lines 269, 454, 463
* **Discrepancy Description:** The CLI takes `textbook|stop|frontier`, but the enum value is `stop_when_stable`. The row names `bellman_ford_pass_<variant>` and `rounds_<variant>` do not say which spelling they use.
* **Rationale & Recommendation:** Use one spelling everywhere, or state the mapping once in §8.

#### structural-2-5: `--k` and `--seed` have no stated defaults - Severity: Nit
* **Reviewer Lens:** `egm-structural` (merged: the defaults part of `design-2-11`)
* **Affected Scope:** Lines 110, 456
* **Discrepancy Description:** Every other option in the usage block shows its default, but these two do not. As a result, "k targets" in CUJ 4 has no value.
* **Rationale & Recommendation:** Give the defaults, or say the values come only from `config_part3.csv` (line 503).

#### structural-2-6: The `ok` and `fewer_runs` statuses hard-code 10 runs - Severity: Nit
* **Reviewer Lens:** `egm-structural` (merged: `style-2-6` and the status part of `design-2-12`)
* **Affected Scope:** Lines 488, 491
* **Discrepancy Description:** "With 10 runs" and "1 to 9" are correct only at the default, but `--runs N` and the config's `runs` column can change it.
* **Rationale & Recommendation:** Write "with `--runs` runs" and "1 to `--runs` − 1".

#### structural-2-7: Cross-references are inconsistent, and the round 1 decision-log entry contains no decision - Severity: Nit
* **Reviewer Lens:** `egm-structural` (merged: `style-2-10`)
* **Affected Scope:** Lines 76, 78, 122, 681
* **Discrepancy Description:**
  * `part3_review/goldfish_review_report.md` (line 78) is plain text, while `part3.md` is a link.
  * "Alternative T", "Alternative X" and "see Risks & Mitigations" are not linked.
  * The line 78 entry says only that the fixes "were accepted as a batch", so the reader learns nothing without opening the other file.
* **Rationale & Recommendation:** Link the report, and use anchor links for the alternatives and sections. In the entry, list in one line which design points changed.

#### structural-2-8: The Glossary comes after its terms are first used - Severity: Nit
* **Reviewer Lens:** `egm-structural`
* **Affected Scope:** Lines 157-163 (against lines 55, 92, 98)
* **Discrepancy Description:** Goal 8 and the CUJs use "step", "answer run" and "scaled weight" before the Glossary inside the Technical Plan defines them.
* **Rationale & Recommendation:** Move the Glossary to just after the Overview, or add a forward pointer at each term's first use.

#### structural-2-9: Three rejection reasons restate the chosen option instead of the trade-off - Severity: Nit
* **Reviewer Lens:** `egm-structural`
* **Affected Scope:** Lines 564 (G), 602 (M), 628 (Q)
* **Discrepancy Description:** The reasons "reported in the result", "synchronous rounds with a frontier" and "the weight is parsed from its text" name the choice rather than why the alternative loses. This conflicts with the standard's Alternatives Rule.
* **Rationale & Recommendation:** Add one trade-off clause to each. For example: G, "a cycle can only be found by running, and the tables must show it as a value"; M, "no rounds, so optimization 1 and the round-n rule would not apply"; Q, "rounding would return, and 3-decimal input would be accepted silently". If design-2-1 is resolved with synchronous rounds, check M again.

#### structural-2-10: The pseudocode puts two variants on one line - Severity: Nit
* **Reviewer Lens:** `egm-structural`
* **Affected Scope:** Lines 311, 314
* **Discrepancy Description:** `frontier = [target] (frontier) | all vertices with dist < +inf (other variants)` uses a `|` that reads as an "or" operator or a table cell.
* **Rationale & Recommendation:** Write `if variant == frontier: ... else: ...`, and make line 314 match.

#### style-2-5: The budget rule is ambiguous about "counting the first run" - Severity: Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** Line 461 (against lines 55 (8a) and 163)
* **Discrepancy Description:** "Counting the first run" can mean its time counts toward the budget, or that it counts toward `--runs`. Line 461 guarantees the first run of every step, but the glossary (line 163) gives that guarantee only to answer runs. Goal 8(a), "stays within its time budget", fails whenever a first run alone exceeds the budget.
* **Rationale & Recommendation:** Write the rule as a formula, for example "another run starts while elapsed (including run 1) + longest run ≤ budget and runs < `--runs`". Then align the glossary and Goal 8(a) ("except that run 1 always runs").

#### style-2-7: Goal 6 and CUJ 1 summarize Bellman-Ford time differently - Severity: Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** Lines 53, 92
* **Discrepancy Description:** Goal 6 says "the mean time", while CUJ 1 and line 504 report the mean and the median.
* **Rationale & Recommendation:** Write "mean and median" in Goal 6.

#### style-2-8: "Every pair of random test graphs" reads as pairs of graphs - Severity: Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** Line 52
* **Discrepancy Description:** The phrase can mean pairs of graphs instead of pairs of vertices.
* **Rationale & Recommendation:** Write "on every vertex pair of the random test graphs".

#### style-2-11: The "Reconciliation with the brief" entry packs seven decisions into one sentence - Severity: Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** Line 77
* **Discrepancy Description:** Seven decisions joined by semicolons are hard to scan and to cite.
* **Rationale & Recommendation:** Split the entry into sub-bullets, one decision each.

#### style-2-12: When the Azure run is split per graph is unclear - Severity: Nit
* **Reviewer Lens:** `egm-style` (the `--only` part is merged into structural-2-3)
* **Affected Scope:** Lines 507, 680
* **Discrepancy Description:** Line 507 can be read as "always split" or as "split only after a refusal". Line 680 says "one graph per session if the estimate exceeds 22 h".
* **Rationale & Recommendation:** Write "if the estimate exceeds 22 h, the run is split per graph with `harness_part3.py --only <graph>`, re-arming before each".

#### style-2-14: Short vague phrases and one wrong figure - Severity: Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** Lines 152, 307, 608, 686, 695, 696
* **Discrepancy Description and Recommendation:**
  * Line 152, "(tables, s)": write "(tables, times in seconds)".
  * Line 307, "can only shorten the number of rounds": write "reduce" (see also design-2-1).
  * Line 608, "about 10¹² relaxations": n·m = 6.1 × 10¹¹, so write "about 6 × 10¹¹".
  * Line 686, "breaks AGENTS.md": write "violates the AGENTS.md rule that pushes need the owner's approval".
  * Line 695, "none critical": delete it, because the next sentence states the fact.
  * Line 696, "each item is its own commit": "item" is undefined before line 700, so write "each Implementation Plan item".

---

## Deduplication log

| Kept id | Merged ids | Shared subject |
| --- | --- | --- |
| design-2-1 | style-2-2 | Goal 3 pruning claim |
| design-2-2 | style-2-4 | Floyd-Warshall time estimate and its basis |
| design-2-6 | style-2-15 | weight-magnitude bound |
| design-2-8 | style-2-9 | "bit-identical" and speed-up wording at line 440 |
| design-2-10 | style-2-13 | who skips the calibration Bellman-Ford pass |
| structural-2-6 | style-2-6, design-2-12 (part) | `ok`/`fewer_runs` hard-coded to 10 runs |
| structural-2-7 | style-2-10 | line 78 decision-log entry and link |
| style-2-3 | design-2-12 (part) | 32 GB "smallest size that fits" |
| structural-2-5 | design-2-11 (part) | `--k`/`--seed` defaults |
| structural-2-3 | style-2-12 (part) | undefined `--only` |

The reviewers' claims about existing code (for example `harness_weighted.py:153-169` in design-2-5) are reported as the reviewers stated them. The synthesizer did not re-verify them.
