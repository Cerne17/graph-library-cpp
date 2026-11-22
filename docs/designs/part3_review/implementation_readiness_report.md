# Goldfish Review Report: Implementation Readiness, Round 1 (Stage 9)

**Target:** `docs/designs/part3_design.md` (lines 769-965, "## Detailed Implementation Plan" to the end)
**Contract:** lines 1-768 of the same file (feature spec and Technical Plan)
**Standard:** `resources/document_standards/implementation-readiness-standard.md` (implementation-readiness and codebase-details); style reviewer used its persona's rules only.

## Reviewer Status

| Reviewer | Status | Raw findings (B / M / N) |
| --- | --- | --- |
| `egm-implementation-readiness` | completed | 3 / 15 / 8 |
| `egm-codebase-details` | completed | 2 / 17 / 9 |
| `egm-style` | completed | 0 / 6 / 6 |

After deduplication: **3 Blockers, 19 Majors, 12 Nits** (34 findings).

Finding ids follow `<reviewer>-1-<n>`, using the reviewer whose report covered the finding most fully. The other reviewers that raised the same finding are listed under "Also raised by". Where reviewers disagreed on severity, the report uses the higher one and records the disagreement.

## Re-raised settled findings

None. No reviewer raised a settled finding again. The implementation-readiness reviewer confirmed that the `float` tests (lines 880, 909) match §1 and are settled under N5.

## Stage 8 regressions still present

Six of the seven plan items that `intermediate_zoom_out_report.md` flagged as undoing earlier fixes are still in the plan:

| Plan lines | Regression | Finding |
| --- | --- | --- |
| 840, 899, 903, 927 | Negative-cycle witnesses outside `find_negative_cycle` (M7) | codebase-details-1-1, implementation-readiness-1-3 |
| 855, 906, 919, 965 | Four optimization combinations / `frontier_only` (M6) | style-1-1 |
| 860 | Fixed timeout instead of the watchdog (M4) | implementation-readiness-1-2 |
| 941 | Budget rule tested in Python (M9) | implementation-readiness-1-6 |
| 865 | 32 GB VM default | implementation-readiness-1-10 |
| 965 | All-pairs always `extrapolated` | style-1-2 |
| 880, 909 | `float` | Consistent with §1; settled |

---

## Blockers

### implementation-readiness-1-1: Commits 6, 7 and 8 do not build as specified - Severity: Blocker
* **Also raised by:** `egm-codebase-details`
* **Affected Scope:** lines 781-785, 885, 890, 899-905, 908
* **Problem:**
  * Commit 6 (line 890) asserts `!parent_tree<target_paths<double>>`, but `target_paths` first appears in commit 7.
  * The commit 7 tests at lines 900, 903, 904 and 905 call `find_negative_cycle`, which only arrives in commit 8.
  * The §3 compile tests for `bellman_ford_on_reversed` and `bellman_ford(reversed(g), t)` (contract line 264) are in item 18, which is commit 5, but they need commit 7.
  * No test cases are assigned to commit 8.
* **Rationale:** Line 771 and the standard require every commit to build and pass `make test`. Contract line 404 only requires the path commit to land before Floyd-Warshall.
* **Fix:**
  * Delete the `target_paths` assert from line 890. Line 908 already has it in `test_bellman_ford.cpp`.
  * Split item 20 into commit 7 cases and commit 8 cases. The `find_negative_cycle` cases go in commit 8.
  * Move the Bellman-Ford rejection tests from §3 into commit 7.

### implementation-readiness-1-2: The harness item contradicts §10 and leaves restart unspecified - Severity: Blocker
* **Also raised by:** `egm-codebase-details`, `egm-style`
* **Affected Scope:** line 860 (and 941)
* **Problem:**
  * `launch(mode, …)` streams "with a timeout". §10 (line 550) replaces fixed timeouts with a watchdog that kills a launch only after max(2 × budget, 5 × the longest gap) without a row.
  * `parse_row` handles "the five row kinds". §8 (lines 501-511) defines nine: `info`, `time`, `calib`, `fit`, `progress`, `path`, `cycle`, `check`, `done`.
  * `completed_steps(results_csv)` resumes from the results CSV. §10 (line 544) keys launches by (graph, mode), and cross-check launches by (graph, `bf_check`, target). It counts a key as complete only when its `done` row is present, and discards rows of incomplete attempts using an attempt id. The schema at line 551 has no attempt column, so the plan does not say where that id is stored.
  * The item leaves out:
    * the imports from `harness_weighted.py` (line 538);
    * its own `check_setup` (line 539);
    * the `bf_check` launches and the `fw_vs_bf` comparison (line 547), and keeping `bf_check` rows out of the statistics;
    * the `estimate` rows and their formulas (line 546);
    * the ETA printing (line 550);
    * `--no-bf-pass` (line 548);
    * the `skipped` rows for the matrix storage (line 490);
    * the `reverse_mean_s` metric (line 563);
    * the order in which modes are launched for each graph.
* **Rationale:** Stage 8 regression (M4). Followed literally, the plan builds the harness that earlier rounds rejected. It also breaks the Zero-Decision Rule.
* **Fix:** Rewrite item 13 as a list of functions, each with its signature and behaviour:
  * a watchdog launch;
  * `parse_row` over the nine row kinds;
  * launch keys as in §10, with completion detected by the `done` row;
  * where the attempt id lives: an `attempt` column in both CSVs, or one file per launch, as Stage 8 Major 2 suggests. Follow whichever §10 finally states;
  * `bf_check` / `fw_vs_bf`, `estimate`, ETA and `--no-bf-pass`;
  * each imported helper, by name;
  * the per-graph launch order.

### implementation-readiness-1-3: The Floyd-Warshall item is out of step with §7 and leaves the kernel choice open - Severity: Blocker
* **Also raised by:** `egm-codebase-details` (its M6 and N2), `egm-style` (its finding 8)
* **Affected Scope:** lines 840, 936 (contract lines 437-443, 461-466)
* **Problem:**
  * The signature is written as `floyd_warshall(g, negative_loops)`, which leaves out `OnPivot` / `no_op_pivot`.
  * The item does not mention:
    * `on_cycle_` and `minus_infinity`;
    * the `pred(i, i) = 0` reset;
    * skipping row i = k;
    * `tree_from(...) const&& = delete`.
  * It adds a witness walk that §7 does not have. That regression is covered by codebase-details-1-1.
  * Contract line 466 requires two kernels, branchless and branchy, timed by `calibrate` at n = 4,000 as `calib,floyd_warshall_branchless,…` and `calib,floyd_warshall_branchy,…`. Both must be bit-identical to a plain loop in `tests/test_floyd_warshall_differential.cpp`, and there is a one-off vectorization check. The plan does not say:
    * where the kernels live;
    * how `calibrate` calls them;
    * how the chosen kernel becomes the one `floyd_warshall` uses;
    * where the plain reference loop lives.
  * Line 936 has no bit-identity test.
* **Rationale:** Zero-Decision Rule. The implementer cannot tell whether to write one kernel or three. Stage 8 Major 1 recommends cutting this to one kernel. Whichever way the owner decides, the plan has to state it.
* **Fix:**
  * Write the full §7 signature and list the members above.
  * Specify the kernel mechanism, for example `namespace detail { enum class fw_kernel { plain, branchless, branchy }; template <fw_kernel K, ...> void fw_run(...); }`, with `floyd_warshall` fixed to the kernel the owner picks after calibration.
  * Name the `calib` rows.
  * Add the bit-identity test to item 23 and put the vectorization check in the Verification Plan.
  * If §7 is cut to one kernel, say so here instead.

---

## Majors

### codebase-details-1-1: Negative-cycle witnesses are back in Bellman-Ford and Floyd-Warshall - Severity: Major
* **Also raised by:** `egm-implementation-readiness` (its M2 and M3), `egm-style` (its findings 8 and 9, rated Nit)
* **Affected Scope:** lines 835, 840, 899, 903, 927
* **Problem:**
  * Line 835 puts `detail::pointer_cycle` in commit 7.
  * Line 840 adds a Floyd-Warshall witness walk.
  * Line 899 expects "witness is {2, 3} or {3, 2}", and line 903 expects "witness {2}". Both sit in Bellman-Ford tests, but `target_paths` has no witness (contract lines 275-281).
  * Line 927 expects Floyd-Warshall witnesses "{1, 2} in some rotation" and "{3}". `all_pairs` exposes only the boolean `negative_cycle()` (line 431).
* **Rationale:** Contract line 365 says `negative_cycle_report::cycle` is "the only witness in the library". This is a Stage 8 regression (M7).
* **Fix:**
  * Delete the witness walk from item 9.
  * Move `detail::pointer_cycle` to commit 8.
  * Lines 899 and 903: write the checks as `find_negative_cycle(g).cycle` and `find_negative_cycle(g, loops).cycle`, compared up to rotation, and place them in commit 8.
  * Line 927: check `negative_cycle() == true`, plus the per-pair checks from implementation-readiness-1-5.

### style-1-1: The optimization variants have the wrong names and count - Severity: Major
* **Also raised by:** `egm-implementation-readiness`, `egm-codebase-details`
* **Affected Scope:** lines 835, 855, 906, 919, 965
* **Problem:**
  * Line 855: `bf_optimizations=both|frontier|stop|off`.
  * Line 906: "both optimizations off", "only `stop_when_stable`" and `frontier_only`, which is defined nowhere.
  * Line 919: "All four option combinations".
  * Line 965: "the four optimization variants".
* **Rationale:** Contract lines 52, 287 and 484 define exactly three values: `textbook | stop_when_stable | frontier`. As written, the harness would pass values the binary rejects. This is a Stage 8 regression (M6).
* **Fix:**
  * Line 855: `bf_optimizations=textbook|stop_when_stable|frontier` for W_1 and W_2. Say that the harness turns `|` into `,` for `--optimizations`, and that the same applies to `bf_sources` / `--sources` and `fw_set` / `--set`.
  * Line 906: textbook 100 rounds / 9,900 relaxations; stop_when_stable 2 / 198; frontier 2 / 99. The implementation-readiness reviewer checked these numbers by hand.
  * Lines 919 and 965: "three variants".
  * Line 835: add `bellman_ford_variant` to the commit 7 contents.

### implementation-readiness-1-4: Goal 3's counter inequalities are not tested - Severity: Major
* **Also raised by:** `egm-codebase-details`
* **Affected Scope:** lines 906, 919
* **Problem:** The differential test checks only that the variants agree on dist, next-tree weights and flags.
* **Rationale:** Goal 3 (line 52) and §4 (line 327) require, on every test graph:
  * textbook ≥ stop_when_stable ≥ frontier, in both `rounds` and `relaxations`;
  * equal rounds for stop_when_stable and frontier;
  * strictly less work on the star.
* **Fix:** Assert these on every random graph in item 21.

### codebase-details-1-2: The `reversed` tests contradict §3 and leave out its negative tests - Severity: Major
* **Also raised by:** `egm-implementation-readiness`, `egm-style`
* **Affected Scope:** lines 825, 885, 908
* **Problem:**
  * Line 885 asks for "reversing a reversed graph's CSR back gives the original arcs". That cannot be written: `reversed(reversed(g))` must not compile (lines 250, 260), and `arcs_` is private (line 237).
  * Contract line 264 lists negative tests for `reversed(reversed(g))` and `bellman_ford(reversed(g), t)`. The plan does not include them, and has no positive or negative checks on `is_reversed_graph`.
  * Item 6 (line 825) does not mention `is_reversed_graph`, its forward declaration, the private constructor or the constrained friend.
  * No negative tests are listed for the `std::floating_point` constraints on `fixed_point_weight`, `target_paths` and `all_pairs`.
* **Rationale:** Goal 7 and AGENTS.md require a negative test for every `requires` clause.
* **Fix:**
  * Line 885: check that the reversed arcs listed by `for_each_edge` equal a hand-written list. Add a local concept for each rejection in line 264.
  * Line 908: add "`bellman_ford(reversed(g), t)` is rejected", in commit 7.
  * Add static_asserts that `fixed_point_weight<int>`, `target_paths<int>` and `all_pairs<int>` are rejected.
  * Item 6: list `is_reversed_graph`, its forward declaration, the private constructor and the friend.

### implementation-readiness-1-5: Per-pair `minus_infinity` and other public members of `all_pairs` are untested - Severity: Major
* **Also raised by:** `egm-codebase-details`
* **Affected Scope:** lines 926-931, 936
* **Problem:** Nothing tests:
  * `minus_infinity(i, j)` on affected and unaffected pairs;
  * the `pred(i, i) == 0` reset after a cycle;
  * the diagonal seed from a negative self-loop (line 453);
  * `tree_from` on unaffected cells of a graph with a cycle;
  * `on_pivot` being called n times with `rows_not_skipped`;
  * `row_tree` satisfying `parent_tree`;
  * `vertex_count()`.

  Line 936 compares only the graph-level `negative_cycle()` flag against Bellman-Ford.
* **Rationale:** Goals 4, 5 and 7, §7 (lines 429-433, 461-463, 470), and the AGENTS.md rule that every public member must be exercised by a test.
* **Fix:**
  * Item 22: a hand-built graph with one affected pair and one unaffected pair, a self-loop case, an `on_pivot` counter test, and `static_assert(parent_tree<all_pairs<double>::row_tree>)`.
  * Item 23: on graphs with planted cycles, assert `minus_infinity(i, j) == (bellman_ford(g, j).dist[i] == -inf)` for every pair, and that all finite cells are equal.

### implementation-readiness-1-6: The budget rule is tested in Python, but it lives in the binary - Severity: Major
* **Also raised by:** `egm-codebase-details`, `egm-style`
* **Affected Scope:** line 941
* **Problem:** The test "a fake run sequence of 100 s each stops after 6 runs" assumes the harness implements the rule. Nothing tests the binary's rule.
* **Rationale:** §8 (line 492) puts the rule in the binary; the harness only classifies sample counts (line 545). This is a Stage 8 regression (M9).
* **Fix:**
  * Test the binary end to end, for example `--budget-seconds 0 --runs 10` gives exactly one `time` row per step.
  * Keep the Python test only for classifying given `time` rows: 10 rows give `ok`, 6 give `fewer_runs`.

### implementation-readiness-1-7: The harness test fixture contradicts itself and is unspecified - Severity: Major
* **Also raised by:** `egm-codebase-details`, `egm-style`
* **Affected Scope:** line 941
* **Problem:**
  * The fixture says "10, 20, 30 reach 100 and one source does not", yet the test expects one `unreachable` row among the three path rows.
  * The 120-vertex graph's arcs and the expected distances are not given.
* **Rationale:** Line 771 requires expected values computed by hand.
* **Fix:**
  * Name the source that cannot reach 100, for example "10 and 20 reach 100; 30 does not; one arc is negative".
  * Give the arcs, or a fixed-seed generator rule plus a small core checked by hand.
  * Give the expected distances to two decimals.

### implementation-readiness-1-8: Program and harness behaviours with no test - Severity: Major
* **Also raised by:** `egm-codebase-details`
* **Affected Scope:** line 941
* **Problem:** No test covers:
  * the `cycle` mode and its witness check (`check,witness`, `error`);
  * `check,variants_agree`;
  * `fw` `path` rows with the status `negative_cycle`;
  * `progress` and `done` rows;
  * `out_of_range`;
  * the missing or invalid `--directed` error;
  * exit on a `max_abs_weight` overflow;
  * `allpairs` stopping early with an `extrapolated` estimate;
  * `calibrate` end to end;
  * `bf_check` / `fw_vs_bf` with `mismatch`;
  * the watchdog kill;
  * restart discarding incomplete attempts;
  * `no_memory` written without a launch;
  * `skipped` matrix rows.
* **Rationale:** §§8-10; the standard's Test Plan requirement covers edge cases.
* **Fix:** Add one named case for each behaviour, with its expected rows.

### implementation-readiness-1-9: The benchmark item leaves design choices open - Severity: Major
* **Also raised by:** `egm-codebase-details` (its M13, on the all-pairs order)
* **Affected Scope:** line 845 (contract lines 492-498, 505-507)
* **Problem:** The item does not say:
  * how the `allpairs` "fixed-seed order" is produced. The copied `sample_sources` (benchmark_weighted.cpp:215-237) returns 1..n in id order when k ≥ n, so it cannot produce a seeded permutation;
  * how the c·n³ fit and `points` are computed;
  * how the 1 GB bandwidth copy and `fit,bandwidth_bytes_per_s` are done;
  * how the "progress row at least every 60 s" works inside `on_pivot` and inside `calibrate`;
  * where the `max_abs_weight` check runs, and which exit codes and stderr text are used on errors;
  * how `out_of_range` is checked before the asserting library calls.
* **Rationale:** Zero-Decision Rule.
* **Fix:** Specify each one. For example: `std::shuffle` of 1..n with `std::mt19937_64(seed)`, using the first k targets when the run stops early; least squares through the origin over the named points.

### codebase-details-1-3: `--directed no` in `bf` and `allpairs` is left to the implementer - Severity: Major
* **Affected Scope:** line 845
* **Problem:** `bf` times `reversed(g)` as a `reverse` row (line 494), and `allpairs` performs one reversal (line 497). But `reversed` does not compile for undirected graphs (line 264). The plan does not say what is printed or timed when `--directed no` is given.
* **Fix:** State it. For example: undirected graphs get no `reverse` row, call `bellman_ford_on_reversed(g, t)` directly, and leave `reverse_mean_s` unwritten.

### implementation-readiness-1-10: The Azure item contradicts §10 - Severity: Major
* **Also raised by:** `egm-codebase-details`, `egm-style`
* **Affected Scope:** line 865 (contract lines 553-555)
* **Problem:**
  * The default VM size is 32 GB (E4as_v5, E4s_v5). §10 and the NFR (line 763) use 16 GB `Standard_E2as_v5`, with 32 GB only after calibration shows memory pressure.
  * `create` sets the auto-shutdown from `AZ_SHUTDOWN_HOURS`. §10 says `start` re-arms it to now + calibrated estimate + 3 h, refuses if that is more than 22 h ahead, and splits the run per graph with `--only`.
  * The item has no `status` step (last `progress` row plus ETA) and no `check` step for the owner-approved push.
  * It does not say how `start` reads the calibrated estimate (file name and format), or that `start` launches `harness_part3.py`.
  * `upload` in azure_part2.sh (lines 109-113) rsyncs into `$REMOTE_DIR/data/`, but the Part 3 harness expects its inputs in `data/part3/` (line 539).
* **Rationale:** An implementer following line 865 would provision the wrong VM size. This is a Stage 8 regression. Copying the script is settled; what the copy contains is not.
* **Fix:**
  * `AZ_SIZES` defaults to `Standard_E2as_v5`, then `Standard_E2s_v5`.
  * Specify `start` re-arming the shutdown, with the 22 h refusal and the per-graph split.
  * Name the estimate file that `calibrate` writes and `start` reads.
  * Add `status`.
  * Upload to `$REMOTE_DIR/data/part3/`.

### style-1-2: The acceptance criteria say the all-pairs comparison is always `extrapolated` - Severity: Major
* **Also raised by:** `egm-implementation-readiness` (Major), `egm-codebase-details` (Nit)
* **Affected Scope:** line 965
* **Problem:** CUJ 4 (line 121), the decision log (line 88) and §8 (line 497) let W_1 and W_2 report `ok` when the full measurement fits the budget. As written, a correct measured result would fail acceptance.
* **Fix:** Write "the all-pairs comparison (`ok` where fully measured, otherwise `extrapolated` with the formula)". Also mention the `skipped` matrix rows, and use "three variants" (see style-1-1).

### implementation-readiness-1-11: The default parser's behaviour on a partly numeric token is unspecified - Severity: Major
* **Also raised by:** `egm-codebase-details`
* **Affected Scope:** line 815
* **Problem:**
  * Today, `1 2 0.5x` reads 0.5 and then fails with "unexpected text after the edge" (edge_list.hpp:132-139).
  * Once the whole token goes to a parser, the plan does not say whether the leftover "x" is rejected, or with which message. One choice silently accepts input that fails today; the other changes the message.
  * The plan also does not say what `detail::read_edge_list_impl` receives for unweighted policies, or whether errors are rethrown through `detail::parse_error` (edge_list.hpp:31).
* **Rationale:** Contract line 192 says behaviour and error messages stay the same.
* **Fix:**
  * The default parser throws "unexpected text after the edge" when the token is not fully consumed, and "expected an edge weight" when extraction fails.
  * The parser is used only under `if constexpr (W::is_weighted)`.
  * Errors are rethrown through `detail::parse_error`.
  * Add a test for `"3\n1 2 0.5x\n"`.

### implementation-readiness-1-12: The end-to-end parser tests omit the required template argument - Severity: Major
* **Also raised by:** `egm-codebase-details` (Nit)
* **Affected Scope:** lines 880, 931
* **Problem:** `read_edge_list(tests/data/three_decimals.txt, fixed_point_weight<double>{2})` does not compile, because W appears only in the return type (contract line 185).
* **Fix:** Write `read_edge_list<gl::weighted<double>>(...)` in both places.

### implementation-readiness-1-13: `fixed_point_weight` can overflow `long long` before the range check runs - Severity: Major
* **Also raised by:** `egm-codebase-details`
* **Affected Scope:** lines 820, 880
* **Problem:**
  * "Accumulates digits in `long long`, rejects magnitudes above 2^digits" does not say the check runs on every digit. A long token overflows first, which is undefined behaviour and is caught by `-fsanitize=undefined`.
  * The overflow tests do not state `decimals`.
  * There is no boundary case.
* **Fix:**
  * Check `acc > (limit - digit) / 10` before each step, counting the fractional padding to `decimals`.
  * State `decimals = 0` for "9007199254740993" and "16777217".
  * Add "9007199254740992" as accepted, and a 25-digit token as rejected.

### implementation-readiness-1-14: The Makefile split between commits 11 and 12 is unspecified, and `make test-part3` fails after commit 11 - Severity: Major
* **Also raised by:** `egm-codebase-details`
* **Affected Scope:** lines 787-788, 850, 962
* **Problem:**
  * Item 11 describes all the Makefile changes as one block, but the commit table puts the Makefile in both commit 11 and commit 12.
  * If `test-part3` lands in commit 11, `unittest discover` finds no `test_harness_part3.py` and exits non-zero on Python 3.12 and later. Yet line 962 requires `make test-part3` to pass after commit 11.
  * The fixture `tests/data/part3_small.txt` arrives in commit 11 with no test that uses it.
* **Fix:** Use one of these:
  * commit 11 adds `benchmark-part3` and `benchmark_part3_checked`, and commit 12 adds `test-part3`, the fixture and `.PHONY`;
  * or line 962 reads "after commits 12 and 13", and commit 11 runs only `make benchmark-part3` plus a smoke run.

### codebase-details-1-4: Commit 1 leaves the review reports out - Severity: Major
* **Also raised by:** `egm-implementation-readiness` (Nit)
* **Affected Scope:** line 777
* **Problem:** The Decision log (lines 87-89) links `docs/designs/part3_review/*.md`, but commit 1 commits only `part3.md` and `part3_design.md`, so those links would point at untracked files. The plan also does not say that the untracked root `design_draft.md` and the modified `presentation/parte2/*` files stay out of every commit.
* **Fix:** List the review reports explicitly in commit 1, and state the exclusions.

### codebase-details-1-5: No commit includes `docs/results/part3_tables.md` - Severity: Major
* **Also raised by:** `egm-implementation-readiness` (Nit)
* **Affected Scope:** lines 789-791, 870
* **Problem:** Item 15 writes this file, but no commit lists it. The standard requires an exact file boundary for each commit.
* **Fix:** Add it to commit 15, or state that it is generated and never committed.

### codebase-details-1-6: The cross-module import may fail in the Python tests - Severity: Major
* **Affected Scope:** lines 857-860, 941
* **Problem:** `harness_part3.py` imports from `harness_weighted.py` (line 538). The repository's Python tests load modules by file location (`tests/test_collector.py:16`, `spec_from_file_location`), which does not put `analysis/` on `sys.path`, so a plain `import harness_weighted` would fail under test.
* **Fix:** Specify how the import is done: `sys.path.insert(0, str(ROOT / "analysis"))` in the test, or an explicit sibling import in the module.

---

## Nits

### implementation-readiness-1-15: The `detail::relax_rounds` signature is not given - Severity: Nit
* **Affected Scope:** line 835 (contract lines 374-376)
* **Problem:** The helper serves two callers with different starting states: the target alone, or all-zero distances with every vertex in the first frontier.
* **Fix:** Give the signature, for example `relax_rounds(graph, dist, pred, initial_frontier, variant, stats) -> changed_in_round_n`. Moving `pointer_cycle` to commit 8 is covered by codebase-details-1-1.

### style-1-3: Item 3 describes the design as a draft that still has to be created - Severity: Nit
* **Also raised by:** `egm-implementation-readiness`, `egm-codebase-details`
* **Affected Scope:** lines 807-810
* **Problem:** "(Create)" and "this draft, saved at the path chosen at finalization" are out of date. The file already exists at `docs/designs/part3_design.md`.
* **Fix:** "`docs/designs/part3_design.md` (Add): this design, committed with its final status line; the brief's status line points to it."

### style-1-4: Undirected cases sit inside a directed-only template, and vertex 4 is implicit - Severity: Nit
* **Also raised by:** `egm-implementation-readiness`, `egm-codebase-details`
* **Affected Scope:** lines 894, 898, 904
* **Problem:** The template runs over directed CSR and directed matrix only, yet lines 898 and 904 are undirected cases. The expected `dist {·, 2, 0, 1, inf}` implies a vertex 4 that the fixture never mentions.
* **Fix:** Make 898 and 904 separate undirected cases, and say "vertex 4 isolated, n = 4".

### implementation-readiness-1-16: The witness order is undefined - Severity: Nit
* **Affected Scope:** lines 900, 903, 904, 917
* **Fix:** State that witnesses are compared up to rotation, in arc order, without repeating v0 (contract lines 365-366).

### implementation-readiness-1-17: Positive `parent_tree` checks and `path_to_target` guards are untested - Severity: Nit
* **Also raised by:** `egm-codebase-details`
* **Affected Scope:** lines 890, 896-909
* **Problem:**
  * §6 (line 404) requires `search_tree`, `shortest_paths` and `all_pairs<double>::row_tree` to satisfy `parent_tree`; no test asserts this.
  * Nothing tests that `path_to_target(t)` returns empty when t is on a negative cycle (line 318).
  * Nothing tests the n-step walk guard on a hand-made corrupt `target_paths` (lines 319-320).
* **Fix:** Add these asserts and cases.

### implementation-readiness-1-18: `decide_fw` is tested at 32 GB - Severity: Nit
* **Affected Scope:** line 941
* **Fix:** Test at 16 GB, the VM size in line 553, and keep the 1 GB margin from line 527.

### style-1-5: The `git stash drop` wording and timing are unclear - Severity: Nit
* **Also raised by:** `egm-implementation-readiness`, `egm-codebase-details`
* **Affected Scope:** line 793
* **Problem:** "(owner told first)" does not say whether the owner's approval is needed. The drop is also scheduled after commit 7, but the prototype's pointer-cycle walk is rewritten in commit 8 (line 576).
* **Fix:** "After commit 8, and with the owner's approval, `git stash drop` removes the parked prototype."

### codebase-details-1-7: Memory-check details are missing - Severity: Nit
* **Affected Scope:** line 860
* **Problem:**
  * `fw_bytes_needed(n)` takes only n, while line 542 says it follows `matrix_bytes_needed(graph)`, which adds the file size (harness_weighted.py:146-150).
  * `decide_fw` does not state the 1 GB margin or where the memory figure comes from.
  * The `--data` default (`data/part3/`) is not stated.
* **Fix:** State the signatures, the margin and the default.

### codebase-details-1-8: The recording-parser test does not name its input - Severity: Nit
* **Affected Scope:** line 875
* **Problem:** The test expects `"0.5"` and `"-1.5"`, but `tests/data/weighted_small.txt` holds 0.5 and 1.5. The existing `read_string` helper (test_edge_list.cpp:44-49) only calls the one-argument overload.
* **Fix:** Give the inline input, and specify a helper variant that takes a parser.

### codebase-details-1-9: Exact floating-point comparisons are not justified - Severity: Nit
* **Affected Scope:** lines 916, 931, 936
* **Problem:** AGENTS.md requires `doctest::Approx` except for uncomputed values. These tests compare computed integer-valued sums exactly, and line 936 does not say whether the comparison with Dijkstra is exact or uses Approx.
* **Fix:** Add one line stating the integer-sum exception and citing the decision log.

### style-1-6: The guard-removal rule is stated twice - Severity: Nit
* **Affected Scope:** lines 771, 963
* **Fix:** Remove the sentence from line 771 and keep line 963, which also says the result is recorded in the commit message body.

### style-1-7: "would take hours" is not quantified - Severity: Nit
* **Affected Scope:** line 855
* **Fix:** "empty for W_3 to W_5 (textbook needs about n·m relaxations, 6 × 10¹¹ on W_3; Alternative N)".

---

## Reuse note (no finding)

`analysis/azure_watchdog.py` already polls the VM for the Part 2 run status. It could serve as the model for `azure_part3.sh status` and the ETA (codebase-details).
