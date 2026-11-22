# Goldfish Annotated Review Report

**Status:** resolved (2026-11-22): Issues 1-3 applied to `part3_design.md` lines 1-776 (14 replacements and Alternative Z); Issue 4 applied by replacing the Detailed Implementation Plan (now lines 777-1010). The owner then closed the review loop after this round.

**Target:** `docs/designs/part3_design.md` (lines 769-965, Detailed Implementation Plan; Issues 1-3 also edit lines 1-768)

**Setup:** Stage 9 round 1, findings from [implementation_readiness_report.md](implementation_readiness_report.md) (3 blockers, 19 major, 12 nits), plus the three open major findings of Stage 8 ([intermediate_zoom_out_report.md](intermediate_zoom_out_report.md)), which change the Technical Plan as well as the plan. The Stage 7 round 3 resolution report was renamed to [goldfish_review_report_stage7_round3.md](goldfish_review_report_stage7_round3.md); its decision-log link is updated with Issue 4.

**Counts:** 37 findings (34 + 3). Issues 1-3 settle the Stage 8 findings (PROPOSE FIX each, owner may overrule). Issue 4 resolves all 34 Stage 9 findings with one rewrite of the Detailed Implementation Plan (PROPOSE FIX); the table under Issue 4 shows how each finding is handled. 0 CONTEST, 0 ESCALATE, 0 DECLINE.

## Prioritized Findings & AI Suggestions

### [Issue 1] [`egm-zoom-out`] - Severity: Major
*   **Affected Scope:** [Lines 42, 466, 498, 505-506](../part3_design.md#L461-L466) (The Problem; §7 implementation note; §8 `calibrate`; row table) and Stage 9 implementation-readiness-1-3
*   **Discrepancy Description:** Stage 8 Major 1. `calibrate` races two Floyd-Warshall kernels on the VM, requires a bit-identical reference loop, and measures a 1 GB bandwidth figure nothing uses; line 466 names row kinds the row table lacks.
*   **Rationale:** A small performance lab the case studies do not need; it also leaves the plan unable to say how many kernels to write (Stage 9 blocker).
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** One kernel removes the race, the reference loop and two row kinds. The branchy update (write only improved cells) is the class algorithm written out, needs no masking, and writes less memory late in a run; with the row and the `dist(i, k)` value hoisted it is still a simple loop. The bandwidth measurement only fed a second estimate; the c·n³ fit and the answer run's live ETA already cover the VM budget.
*   ***Proposed Edit:***
    *   Location: The Problem (estimate basis kept as an assumption, not measured); §7 implementation note; §8 `calibrate` and row table; Risks row 2; Alternatives.
    *   Change: §7 note becomes "one kernel: for each k, hoist `dist(i, k)` and pointers to rows i and k (i ≠ k, so they never alias); the j loop updates a cell only when it improves. Vectorization is not required." `calibrate` prints `calib,floyd_warshall,…` and `fit,floyd_warshall_n3,…` only; the bandwidth copy and `fit,bandwidth_bytes_per_s` are removed. New Alternative Z "two kernels raced on the VM, plus a bandwidth model": rejected by author, extra code and rows for at most a constant factor, and the live ETA already guards the budget.

### [Issue 2] [`egm-zoom-out`] - Severity: Major
*   **Affected Scope:** [Lines 543-551](../part3_design.md#L543-L551) (§10 harness: keys, restart, watchdog) and Stage 9 implementation-readiness-1-2
*   **Discrepancy Description:** Stage 8 Major 2. Rows carry attempt ids that no CSV has a column for, and incomplete attempts are discarded on restart; the restart machinery is heavy.
*   **Rationale:** An unimplementable rule (no column), and more state than needed.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** One file per launch makes completion visible on disk and removes attempt ids: the harness tees each launch's stdout to `data/part3_runs/<graph>/<key>.csv`; a file is complete when its last row is `done`; only complete files are merged into `results_part3.csv` and `paths_part3.csv`; an incomplete file is renamed `<key>.csv.incomplete` and the launch rerun. The watchdog becomes one fixed silence limit, because `fw` and `calibrate` now print a row at least every 60 s.
*   ***Proposed Edit:***
    *   Location: §10 harness sub-list (Streaming, Keys and restart, Watchdog); Goal 8(d); Risks row 1.
    *   Change: as in the justification; watchdog "kills a launch when no row arrived for 2 × `--budget-seconds` (20 min by default)". Keys are file names: `fw`, `bf`, `cycle`, `allpairs`, `calibrate`, `bf_check_<t>`.

### [Issue 3] [`egm-zoom-out`] - Severity: Major
*   **Affected Scope:** [Lines 494-547](../part3_design.md#L494-L547) (§8 `bf`, `cycle`; §9 `mismatch`, `error`; §10 cross-check)
*   **Discrepancy Description:** Stage 8 Major 3. Run-time self-checks repeat the tests: `variants_agree`, `witness`, `fw_vs_bf`. The two zoom-out passes disagreed on `witness`.
*   **Rationale:** Run-time checks cost code and output rows; keep only the ones that produce evidence the report shows.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** `variants_agree` is covered by the differential test that runs every variant on random graphs, and the case-study answers come from the default variant only; drop it. `witness` stays: the witness is printed as the evidence of a negative cycle in the results, so checking it before printing guards what the report shows (owner may switch it to a test-only check). `fw_vs_bf` stays: it is the only check of Floyd-Warshall on the real W_1 and W_2.
*   ***Proposed Edit:***
    *   Location: §8 `bf` and row table (`check` names), §9 `mismatch`.
    *   Change: remove `check,variants_agree`; the `check` row names are `witness` (binary) and `fw_vs_bf` (harness); `mismatch` means only the `fw_vs_bf` disagreement.

### [Issue 4] [`egm-implementation-readiness`, `egm-codebase-details`, `egm-style`] - Severity: Blocker
*   **Affected Scope:** [Lines 769-965](../part3_design.md#L769-L965) (the whole Detailed Implementation Plan)
*   **Discrepancy Description:** all 34 Stage 9 findings (3 blockers, 19 majors, 12 nits): commits 6-8 do not build in order; the harness, Floyd-Warshall, benchmark, Azure and reversed-graph entries contradict §§3, 7, 8 and 10; six Stage 8 regressions; untested behaviours; unspecified choices.
*   **Rationale:** The plan was drafted before three critic rounds changed §§1-11 and was never updated.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** With this many contradictions, patching line by line would leave gaps; the plan is rewritten from §§1-11 as they stand after Issues 1-3, keeping its structure (commit table, file entries, verification) and every hand-computed value that still holds. The rewrite makes these choices, so no decision is left to the implementer:
    *   **Commit order (each builds):** 1 docs, review reports, `.gitignore`; 2 `negative_self_loop_edges`; 3 parser hook; 4 `fixed_point_weight`; 5 `reversed_graph` (positive tests and `reversed(reversed(g))` rejected); 6 `parent_tree` and bounded `reconstruct_path` (no `target_paths` assert); 7 Bellman-Ford toward a target (variants, `relax_rounds`, −∞ marking, `path_to_target`; `bellman_ford(reversed(g), t)` rejected; counter ordering); 8 `find_negative_cycle` and `pointer_cycle` (witness tests); 9 Bellman-Ford differential; 10 Floyd-Warshall and its tests; 11 benchmark, Makefile `benchmark-part3` and `benchmark_part3_checked`, smoke run; 12 harness, config, `test-part3` target, fixture and Python tests; 13 Azure script and report script; 14 docs; 15 results and `docs/results/part3_tables.md`. The stash is dropped after commit 8 with the owner's approval. `design_draft.md` and `presentation/parte2/*` are never committed.
    *   **Parser hook:** the default parser throws "unexpected text after the edge" when the token is not fully consumed and "expected an edge weight" when extraction fails; used only under `if constexpr (W::is_weighted)`; errors go through `detail::parse_error`; new test `"3\n1 2 0.5x\n"`.
    *   **`fixed_point_weight`:** the range check runs before every digit (`acc > (limit - digit) / 10`), counting the padding to `decimals`; boundary cases `"9007199254740992"` accepted and `"9007199254740993"` rejected with `decimals = 0`, a 25-digit token rejected.
    *   **Undirected input:** `bf` and `allpairs` call `bellman_ford_on_reversed(g, t)` directly, print no `reverse` row, and `reverse_mean_s` is not written.
    *   **`allpairs` order:** `std::shuffle` of 1..n with `std::mt19937_64(seed)`; the first targets of that order are used when the step stops early.
    *   **Fit:** least squares through the origin of time = c · n³ over the `calib` points.
    *   **Progress every 60 s:** `on_pivot` prints when 250 pivots or 60 s have passed since the last `progress` row; `calibrate` prints one after each run.
    *   **Errors:** `max_abs_weight` is checked right after loading; out-of-range ids are checked by the program before any library call (`out_of_range` row); errors print `error: <message>` on stderr and exit with status 1 (as `benchmark_weighted`).
    *   **Python import:** `harness_part3.py` inserts its own folder into `sys.path` before `import harness_weighted`.
    *   **Calibration output for Azure:** the harness writes `data/part3_runs/vm_estimate.txt` (hours, one number); `azure_part3.sh start` reads it.
    *   **Exact comparisons:** tests on integer-weighted graphs compare exactly, stated once with the decision-log reference; comparisons with Dijkstra on non-integer weights use `doctest::Approx`.
*   ***Proposed Edit:*** the section from `## Detailed Implementation Plan` to the end is replaced. Finding-by-finding handling:

| finding | handled by |
|---|---|
| implementation-readiness-1-1 (commits 6-8) | commit order above; item 20 split between commits 7 and 8 |
| implementation-readiness-1-2 (harness) | harness entry rewritten from §10 and Issue 2: functions, nine row kinds, per-launch files, watchdog, `bf_check` / `fw_vs_bf`, estimates, ETA, `--no-bf-pass`, imports, own `check_setup`, `skipped` matrix rows, `reverse_mean_s`, launch order per graph (`calibrate`, `cycle`, `bf`, `bf` variants, `fw` + `bf_check_<t>`, `allpairs`) |
| implementation-readiness-1-3 (Floyd-Warshall) | full §7 signature and members; one kernel (Issue 1) |
| codebase-details-1-1 (witnesses) | witnesses only from `find_negative_cycle`, tests in commit 8, compared up to rotation |
| style-1-1 (variant names) | three variants everywhere; `|` in config becomes `,` on the command line |
| implementation-readiness-1-4 (counter inequalities) | asserted on every random graph in item 21 |
| codebase-details-1-2 (reversed tests) | tests rewritten against hand-written arc lists; all §3 rejections; `is_reversed_graph` checks; `floating_point` rejections of `fixed_point_weight<int>`, `target_paths<int>`, `all_pairs<int>` |
| implementation-readiness-1-5 (`all_pairs` members) | hand cases for `minus_infinity`, `pred(i, i)` reset, self-loop seed, `tree_from` on unaffected cells, `on_pivot` count, `row_tree` is a `parent_tree`, `vertex_count`; differential `minus_infinity` against Bellman-Ford −∞ |
| implementation-readiness-1-6 (budget rule) | tested end to end on the binary (`--budget-seconds 0` gives one `time` row per step); Python test only classifies counts |
| implementation-readiness-1-7 (fixture) | hand-built `part3_small.txt` with stated arcs and expected distances |
| implementation-readiness-1-8 (untested behaviours) | one named case per behaviour in `test_harness_part3.py` |
| implementation-readiness-1-9 (benchmark choices) | choices listed above |
| codebase-details-1-3 (undirected) | rule above |
| implementation-readiness-1-10 (Azure) | 16 GB default, `start` re-arming, `status`, push step, estimate file, upload to `data/part3/` |
| style-1-2 (all-pairs acceptance) | "`ok` where fully measured, otherwise `extrapolated`" |
| implementation-readiness-1-11 (default parser) | rule above |
| implementation-readiness-1-12 (template argument) | `read_edge_list<gl::weighted<double>>(…)` |
| implementation-readiness-1-13 (overflow) | rule above |
| implementation-readiness-1-14 (Makefile split) | commit 11 / commit 12 split above |
| codebase-details-1-4 (review reports in commit 1) | listed; exclusions stated |
| codebase-details-1-5 (`part3_tables.md`) | committed in commit 15 |
| codebase-details-1-6 (Python import) | rule above |
| implementation-readiness-1-15 (`relax_rounds` signature) | signature given |
| style-1-3 (item 3 wording) | fixed |
| style-1-4 (undirected cases) | separate undirected cases, n = 4 with vertex 4 isolated |
| implementation-readiness-1-16 (witness order) | compared up to rotation, arc order, v0 not repeated |
| implementation-readiness-1-17 (`parent_tree` positives, `path_to_target` guards) | asserts and cases added |
| implementation-readiness-1-18 (`decide_fw` at 32 GB) | tested at 16 GB with the 1 GB margin |
| style-1-5 (stash) | after commit 8, with the owner's approval |
| codebase-details-1-7 (memory check) | `fw_bytes_needed(graph)`, margin, `--data` default |
| codebase-details-1-8 (recording parser input) | inline input; a `read_string_with` helper |
| codebase-details-1-9 (exact comparisons) | rule above |
| style-1-6 (guard rule twice) | kept once, in the Verification Plan |
| style-1-7 ("would take hours") | quantified (6 × 10¹¹ relaxations on W_3) |

The reuse note (`analysis/azure_watchdog.py`) is adopted: `azure_part3.sh status` follows its approach.
