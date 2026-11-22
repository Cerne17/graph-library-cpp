# Goldfish Annotated Review Report

**Status:** resolved (2026-11-22): all 27 PROPOSE FIX items applied to `part3_design.md` lines 1-699; 3 declined; 4 deferred to Stage 9.

**Target:** `docs/designs/part3_design.md` (lines 1-628, FeatureRange)

**Setup:** findings from Stage 7 round 1 ([critic_report.md](critic_report.md), 44 findings), merged with the still-open findings of Stage 5 ([zoom_out_report.md](zoom_out_report.md)) and Stage 6 ([comprehension_report.md](comprehension_report.md)) that fall in lines 1-628. Duplicates are merged into one issue, with every source id listed. Findings that only touch the Detailed Implementation Plan (line 629 on) are listed at the end as **deferred to Stage 9**; their in-range parts are fixed here.

**Counts:** 30 issues: 27 PROPOSE FIX, 0 CONTEST, 0 ESCALATE, 3 DECLINE; plus 4 deferred to Stage 9. Every PROPOSE FIX also lists the follow-up edits it implies in the Detailed Implementation Plan, to be made in Stage 9 so the two ranges agree.

## Prioritized Findings & AI Suggestions

### [Issue 1] [`egm-design`] - Severity: Blocker
*   **Sources:** design-1-1.
*   **Affected Scope:** [Lines 276-298](../part3_design.md#L276-L298) (Technical Plan §4, round pseudocode and "When round n still changes something")
*   **Discrepancy Description:** The self-loop rule (step 3) only runs inside "when round n still changes something". Negative self-loops are not CSR arcs, so when a self-loop is the only negative cycle reaching the target the rounds settle early, line 287 stops with "no cycle reaches target", and vertices that reach the loop keep finite distances.
*   **Rationale:** Reintroduces the bug brief finding B1 was raised to fix; breaks Goal 4.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Verified against the pseudocode: step 3 is unreachable when the rounds end early. The −∞ marking argument stays sound if the seed set is built after the loop ends for any reason.
*   ***Proposed Edit:***
    *   Location: pseudocode line 287 and the paragraph "When round n still changes something" with its steps 1-3.
    *   Change: line 287 becomes `if changed_this_round is empty and (stop_when_stable or frontier_only): stop`. The paragraph is replaced by "**After the rounds end** (early or at round n), the seeds are the vertices changed in round n (none if the loop stopped early) plus every negative-self-loop vertex v with `dist[v] < +inf`. If there are no seeds, no negative cycle reaches the target. Otherwise: 1. mark −∞ (BFS in `rev` from the seeds; unchanged argument for the round-n seeds; a self-loop vertex that reaches the target has distance −∞ by definition, and so has every vertex that reaches it); 2. `cycle_reaches_target = true`." Steps 1 (witness) and 3 (self-loops) are removed (witnesses: see Issue 4). Plan follow-up: a regression test whose only negative cycle is a self-loop on a vertex that reaches the target, with finite rounds stopping early, shown to fail before the fix.

### [Issue 2] [`egm-structural`, `egm-design`] - Severity: Blocker
*   **Sources:** structural-1-1, design-1-8.
*   **Affected Scope:** [Lines 466-625](../part3_design.md#L466-L625) (new section between Alternatives Considered and Non-Functional Requirements)
*   **Discrepancy Description:** No Risks and Mitigations section; risks are scattered across eight places and some are not handled: a dying Floyd-Warshall answer run, auto-shutdown or credit ending it, a biased calibration, a 5× time uncertainty, unknown cycle status of W_1, the release-build `tree_from` assertion, the reversal's temporary memory on W_5, and the push the Azure run needs.
*   **Rationale:** feature-spec-standard requires failure modes and mitigations; brief M16 asked for the section.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid. The push is a real gap: `azure_part2.sh setup` clones the branch from GitHub, and AGENTS.md requires the owner's approval for any push.
*   ***Proposed Edit:***
    *   Location: new `## Risks & Mitigations` before `## Non-Functional Requirements`.
    *   Change: a table (risk, trigger, impact, mitigation, where handled, who decides) with rows: (1) Floyd-Warshall answer run dies or is deallocated mid-run → hours lost → progress rows and a watchdog, auto-shutdown re-armed before `start`, one graph per session if the estimate exceeds 22 h (Issue 6); (2) calibration fit wrong at n = 25,000 → VM budget wrong → the answer run's own progress rows give a live ETA after about 10 minutes, owner may stop it (Issue 5); (3) directed W_1 has a negative cycle → case studies 1 and 2 partly −∞ → per-pair classification in both algorithms (Issues 1, 3); (4) false negative cycle from rounding → fixed-point weights, path-sum bound stated (Issue 22); (5) `tree_from` guard compiled out → program calls paths only for `ok` pairs; `reconstruct_path` bounded (Issue 3); (6) the reversal's temporary edge copy on W_5 (about 640 MB) → fits the 32 GB VM; stated; (7) the Azure run needs `feature/part3` on GitHub → the owner approves the push before `setup`, listed as a step in §10 (Issue 18); (8) student credit runs out → calibration estimate approved before `start`, VM deleted after `fetch`. Plan follow-up: none.

### [Issue 3] [`egm-design`, `egm-zoom-out`, `egm-inventive-catalyst`, `egm-comprehension`] - Severity: Major
*   **Sources:** design-1-2, design-1-17, Stage 5 M1 and N1, Stage 6 M1 and M6.
*   **Affected Scope:** [Lines 79](../part3_design.md#L79), [372-401](../part3_design.md#L372-L401), [441-445](../part3_design.md#L441-L445), [610](../part3_design.md#L610) (CUJ 2, §7, §9, Alternative U)
*   **Discrepancy Description:** With a negative cycle anywhere, Floyd-Warshall leaves every pair "unspecified", so case study 2 has no answers, the `fw` path rows have no status, `tree_from`'s assertion vanishes in release builds, and no mode runs the Bellman-Ford cross-check behind `mismatch`.
*   **Rationale:** All three stages raised it independently; it decides whether case study 2 has answers on W_1.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid. Without an early stop, a pair (i, j) is −∞ exactly when some k has dist(i, k) < +inf, dist(k, k) < 0 and dist(k, j) < +inf: each negative cycle's highest-numbered vertex gets dist(k, k) < 0 by the end, and a negative closed walk through k means k lies on or reaches a negative cycle. Every other pair, and its predecessor chain, only ever combined values of unaffected pairs, so it is exact.
*   ***Proposed Edit:***
    *   Location: §7 class sketch, the paragraph at line 401, §8 `fw` bullet, §9 table, CUJ 2, Alternative U.
    *   Change: (a) `all_pairs` gains `bool minus_infinity(vertex_id i, vertex_id j) const` (O(number of k with dist(k, k) < 0), computed once into a list after the main loop); `dist(i, j)` documented as meaningful only when `!minus_infinity(i, j)`. (b) Line 401: "With a negative cycle, the pairs with `minus_infinity(i, j)` have no defined distance; every other pair and its predecessor chain are exact. `tree_from(i)` is valid only for a row without such pairs; it asserts in debug builds, and the program never calls it for other rows." (c) §8 `fw`: for each ordered pair of `--set`, a `path` row with status `ok`, `unreachable` or `negative_cycle`; then, untimed, one `bellman_ford_on_reversed` toward each vertex t of the set, compared with column t of Floyd-Warshall (classification and exact value); any difference gives a `check` row with status `mismatch` (Issue 9). (d) §9: `negative_cycle` covers "Bellman-Ford −∞ or Floyd-Warshall `minus_infinity`". (e) CUJ 2: add "if the graph has a negative cycle, only the affected pairs say `negative_cycle`". (f) Alternative U's last sentence points to (c). Plan follow-up: tests reusing the Bellman-Ford "cycle reaching the target, partly" graph pair by pair; planted-cycle differential test against `bellman_ford` classifications.

### [Issue 4] [`egm-design`, `egm-zoom-out`] - Severity: Major
*   **Sources:** design-1-3, Stage 5 M7.
*   **Affected Scope:** [Lines 236-238](../part3_design.md#L236-L238), [296-298](../part3_design.md#L296-L298), [319](../part3_design.md#L319), [360-375](../part3_design.md#L360-L375), [398](../part3_design.md#L398), [433](../part3_design.md#L433)
*   **Discrepancy Description:** Three producers of negative-cycle witnesses, only one printed, none checked, and the `cycle` row has no weight.
*   **Rationale:** A wrong witness (non-negative, or not made of arcs) would be shown as proof; the extra producers cost code and tests the assignment does not ask for.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid on both counts; dropping two producers removes most of the checking work.
*   ***Proposed Edit:***
    *   Location: `target_paths` sketch, §4 steps (already rewritten by Issue 1), `all_pairs` sketch and algorithm, §8 row table.
    *   Change: remove `target_paths::cycle`, `all_pairs::cycle_` and `cycle()`, and the Floyd-Warshall witness line; keep the booleans `cycle_reaches_target` and `negative_cycle()`. The only witness is `negative_cycle_report::cycle`. §8: before printing, the program checks that each consecutive pair of the witness (and the closing pair) is an arc of g (lightest parallel arc) or a negative self-loop, and sums the weights; the row becomes `cycle,<found>,<length>,<weight>,<vertices>,<status>` with status `ok`, or `error` if the check fails or the sum is not negative. Plan follow-up: randomized planted-cycle tests check the witness of `find_negative_cycle`.

### [Issue 5] [`egm-design`, `egm-zoom-out`, `egm-inventive-catalyst`, `egm-comprehension`, `egm-style`] - Severity: Major
*   **Sources:** design-1-4, design-1-5, Stage 5 M2 and M3, Stage 6 M2.
*   **Affected Scope:** [Lines 47](../part3_design.md#L47), [79](../part3_design.md#L79), [103](../part3_design.md#L103), [420-424](../part3_design.md#L420-L424), [442-446](../part3_design.md#L442-L446)
*   **Discrepancy Description:** The 10-run Floyd-Warshall mean comes from a random-graph fit although a full-size run is measured; the fit is biased (cache, reachability); a step with exactly one measured run has no status; where the extrapolated mean is computed is unstated.
*   **Rationale:** Two disagreeing figures for one quantity, and a VM budget built on a biased model.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid. Floyd-Warshall's time is nearly deterministic for a given graph, so the measured run is the best estimate of its mean; the answer run's own progress also replaces a separate k-block calibration on the real graph (the brief's B6 idea) at no cost.
*   ***Proposed Edit:***
    *   Location: Goal 8, CUJ 2, CUJ 6, §8 `fw` and `calibrate`, §9, Alternatives.
    *   Change: (a) `fewer_runs` covers 1 to 9 measured runs (the `samples` column gives the count; 1 sample has no spread). (b) Floyd-Warshall's reported time on W_1 and W_2 is the measured answer run (`fewer_runs`, samples = 1). (c) The c·n³ fit (random graphs) is used only before the run, to estimate VM time, and for the not-run W_3 to W_5 (status `extrapolated`, formula in the note). (d) `fw` prints a flushed `progress,floyd_warshall,<k>,<elapsed_micros>,<rows_not_skipped>` row every 250 pivots through an optional no-op observer `on_pivot(k, rows_not_skipped)` on `floyd_warshall`; the harness shows the ETA from them (a lower bound in the first minutes), and the owner may stop the run after about 10 minutes if the ETA exceeds the approved budget. (e) The harness, not the binary, writes `estimate` rows into `results_part3.csv`, combining the calibration output with each step's rows. (f) New alternative "a separate k-block calibration run on the real graph": rejected by author, since the answer run's progress rows give the same measurement without a second run. Plan follow-up: test that the observer is called n times; `progress` in `parse_row`.

### [Issue 6] [`egm-design`, `egm-inventive-catalyst`, `egm-comprehension`] - Severity: Major
*   **Sources:** design-1-6, Stage 5 M4, Stage 6 M4.
*   **Affected Scope:** [Lines 47](../part3_design.md#L47), [65](../part3_design.md#L65), [104](../part3_design.md#L104), [455-457](../part3_design.md#L455-L457), [623](../part3_design.md#L623)
*   **Discrepancy Description:** `az vm auto-shutdown` is a daily schedule (at most about 24 h ahead) set at `create`; the upper estimate is about 20-22 h; deallocation kills the run; the per-launch timeout has no value and a fixed one either kills the answer run or never catches a hang.
*   **Rationale:** The safety net can end the run that matters most.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid; with Issue 5's progress rows a heartbeat replaces the fixed timeout.
*   ***Proposed Edit:***
    *   Location: §10 harness and Azure bullets, Goal 8, CUJ 6, NFR cost bullet.
    *   Change: (a) no fixed per-launch timeout: a watchdog kills a launch only when no row (`time`, `progress` or `done`) arrived for max(10 min, 5 × the longest gap seen so far). (b) `azure_part3.sh start` re-arms the auto-shutdown to now + calibrated estimate + 3 h and refuses to start if that exceeds 22 h; then the run is split per graph (`--only`), re-arming before each. (c) `azure_part3.sh status` prints the last `progress` row and the ETA.

### [Issue 7] [`egm-design`] - Severity: Major
*   **Sources:** design-1-7, style-1-16.
*   **Affected Scope:** [Lines 129](../part3_design.md#L129), [425](../part3_design.md#L425), [455-456](../part3_design.md#L455-L456)
*   **Discrepancy Description:** "Complete" (graph, mode) pairs are skipped on restart, but "complete" is undefined; rows of a died attempt mix with a rerun; two different flush rules.
*   **Rationale:** Resume without a protocol corrupts means.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: §8 output paragraph, data-flow diagram label, §10 harness bullet.
    *   Change: the binary prints `done,<mode>` as its last row; one flush rule, "flushed after every row". The harness tags every row with an attempt id, treats a (graph, mode) pair as complete only when its `done` row is present, and on restart discards the rows of incomplete attempts before rerunning.

### [Issue 8] [`egm-structural`, `egm-style`, `egm-comprehension`] - Severity: Major
*   **Sources:** structural-1-2, style-1-4, Stage 6 M3.
*   **Affected Scope:** [Lines 45](../part3_design.md#L45), [54](../part3_design.md#L54), [85](../part3_design.md#L85), [443-444](../part3_design.md#L443-L444)
*   **Discrepancy Description:** Floyd-Warshall on W_3 to W_5 is "skipped", "`no_memory` (or `skipped` for time)" and `no_memory` in three places.
*   **Rationale:** The status table is the contract between binary, harness and report.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: Non-Goals, CUJ 3, §9, Goal 6.
    *   Change: precedence rule in §9: "the memory check runs first: a step that does not fit gets `no_memory` and is not launched; `skipped` is only for steps turned off in the config (or the matrix storage) that would fit". Non-Goals: "reported as `no_memory`"; CUJ 3: drop "(or `skipped` for time)"; Goal 6: "every row that is not `ok` gives its status from §9 and the reason".

### [Issue 9] [`egm-structural`, `egm-style`] - Severity: Major
*   **Sources:** structural-1-3, structural-1-4, style-1-5, style-1-18.
*   **Affected Scope:** [Lines 47](../part3_design.md#L47), [73](../part3_design.md#L73), [91](../part3_design.md#L91), [420-448](../part3_design.md#L420-L448)
*   **Discrepancy Description:** `extrapolated` overlaps `fewer_runs`; CUJ 1 promises 10 runs; the status table's "row kinds" do not match the row schema (`time` rows have no status, variant mismatches have no row, "results (harness)" is not a row kind, `cycle` rows have no status, the `time` algorithm list omits the variants).
*   **Rationale:** The two tables are the interface specification.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: §8 row table, §9 table, CUJ 1.
    *   Change: `extrapolated` = "not measured; computed from a model (calibration fit, or reversal + n × mean pass time), formula in the note"; `fewer_runs` = 1 to 9 measured runs (Issue 5). New row kind `check,<name>,<status>,<detail>` for cross-checks (`variants_agree`, `fw_vs_bf`, `witness`), status `ok` / `mismatch` / `error`. `cycle` gets a status column (Issue 4); `time` rows have none. §9 column renamed "emitted in", with values such as `stdout: path`, `stdout: check`, `results_part3.csv`. `time` algorithm list adds `bellman_ford_pass_<variant>`. CUJ 1: "over up to 10 runs, with the sample count".

### [Issue 10] [`egm-zoom-out`, `egm-design`] - Severity: Major
*   **Sources:** Stage 5 M6, design-1-12.
*   **Affected Scope:** [Lines 42](../part3_design.md#L42), [241-245](../part3_design.md#L241-L245), [287](../part3_design.md#L287), [413-420](../part3_design.md#L413-L420)
*   **Discrepancy Description:** The two booleans give three behaviours ("frontier" and "both" are the same run); Goal 3's "each can be switched off" is false for `stop_when_stable` when `frontier_only` is on.
*   **Rationale:** A comparison that measures the same run twice, and a false goal.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid (line 287). The owner's decision to keep switchable optimizations holds; only their shape changes.
*   ***Proposed Edit:***
    *   Location: `bellman_ford_options`, Goal 3, §8 `--optimizations`.
    *   Change: `enum class bellman_ford_variant { textbook, stop_when_stable, frontier }` (frontier implies stopping on an empty frontier); `bellman_ford_options { bellman_ford_variant variant = bellman_ford_variant::frontier; }`. Goal 3: "three variants: the textbook version, optimization 1 alone, and optimizations 1 and 2; tests show each later variant does no more work than the one before, with the same answers". `--optimizations textbook,stop,frontier`. Plan follow-up: config values and the pruning test (9,900 / 198 / 99) map onto the three variants.

### [Issue 11] [`egm-structural`] - Severity: Major
*   **Sources:** structural-1-5, structural-1-6.
*   **Affected Scope:** [Lines 59-65](../part3_design.md#L59-L65), [468](../part3_design.md#L468)
*   **Discrepancy Description:** Rejections cite "the drafting conversation of 2026-11-21", which no reader can open; part3.md is a code span, not a link; Open Questions holds mostly decisions.
*   **Rationale:** The standard requires a verifiable source for attributed rejections.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid; the fix also gives the drafting decisions a home in the document.
*   ***Proposed Edit:***
    *   Location: `## Open Questions` and line 468.
    *   Change: rename the section `## Decisions and Open Questions`, with `### Decision log` (each drafting decision of 2026-11-21 and 2026-11-22 in one line: fixed-point weights; parser hook; generic parser in the library; 600 s step budget; answer runs once; reconciliation choices) and `### Open questions` (only the VM-time budget). Line 468: link [part3.md](part3.md) and the decision log; drop the conversation reference.

### [Issue 12] [`egm-style`] - Severity: Major
*   **Sources:** style-1-1, style-1-6, style-1-2.
*   **Affected Scope:** [Lines 44](../part3_design.md#L44), [64](../part3_design.md#L64), [345](../part3_design.md#L345)
*   **Discrepancy Description:** Line 64 names the constant as 100 (it is the decimals, 2); line 44 says rows work with the "existing" `reconstruct_path` (only after §6); line 345 says a bare `std::span` satisfies `parent_tree` (it needs `source` and `parent`).
*   **Rationale:** Each invites a wrong implementation or test.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** All three verified against §1, `path.hpp:16` and §6.
*   ***Proposed Edit:***
    *   Location: lines 44, 64, 345.
    *   Change: 44: "works with `reconstruct_path` (after the `parent_tree` change in §6) without copying". 64: moved into the decision log (Issue 11) as "one named constant, the number of decimals (2); the scale (100) comes from it". 345: "`all_pairs<W>::row_tree`, whose `parent` is a `std::span<const vertex_id>`, now qualifies too."

### [Issue 13] [`egm-style`] - Severity: Major
*   **Sources:** style-1-3.
*   **Affected Scope:** [Line 20](../part3_design.md#L20) (Overview)
*   **Discrepancy Description:** "Part 1 and Part 2 code and programs stay as they are" is false: §§1, 2 and 6 change library code.
*   **Rationale:** Promises more than the plan does.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: line 20.
    *   Change: "Part 1 and Part 2 programs and results stay as they are. Library changes to existing headers are limited to `io/edge_list.hpp` (§§1-2) and `algorithms/path.hpp` (§6)."

### [Issue 14] [`egm-style`, `egm-zoom-out`] - Severity: Major
*   **Sources:** style-1-7, Stage 5 N3.
*   **Affected Scope:** [Line 47](../part3_design.md#L47) (Goal 8)
*   **Discrepancy Description:** Goal 8 mixes the budget rule, statuses, the 20 s history, answer runs, Azure policy and resilience; "modest" cannot be measured.
*   **Rationale:** A goal states measurable outcomes; the protocol belongs in §8.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: Goal 8.
    *   Change: "**Bounded measurement.** (a) Repeated timing of each step stays within its time budget (default 600 s), reporting the number of runs measured. (b) Answer runs (Bellman-Ford toward 100, Floyd-Warshall among the set) always run once if they fit in memory. (c) Azure: one VM of the smallest size that fits, deleted after `fetch`. (d) A run that dies loses at most its unfinished step. Rules in §8 and §10." The 20 s history stays only in the alternatives.

### [Issue 15] [`egm-inventive-catalyst`] - Severity: Major
*   **Sources:** Stage 5 M5.
*   **Affected Scope:** [Lines 386-398](../part3_design.md#L386-L398) (§7 algorithm)
*   **Discrepancy Description:** The inner loop reads cells through an index function and writes two arrays behind a branch, which compilers vectorize poorly; that constant factor is most of the VM cost.
*   **Rationale:** Cost of the hours-long runs.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid and cheap: hoisting the row pointers and making the j loop branchless gives bit-identical results. Threading is not proposed: it would make the comparison with single-threaded Bellman-Ford unfair, so it goes to the non-goals; the owner can overrule.
*   ***Proposed Edit:***
    *   Location: after the algorithm block in §7; Non-Goals.
    *   Change: "Implementation note: for each k, hoist `dist(i, k)` and pointers to rows i and k of both matrices; the j loop is branchless over contiguous memory (select the new distance and predecessor with a comparison mask). Vectorization is checked once with `-Rpass=loop-vectorize` (clang) or `-fopt-info-vec` (GCC); the speed-up is recorded in the report." Non-Goals: "multi-threaded Floyd-Warshall (it would make the comparison with single-threaded Bellman-Ford unfair)".

### [Issue 16] [`egm-zoom-out`] - Severity: Major
*   **Sources:** Stage 5 M8.
*   **Affected Scope:** [Lines 111-118](../part3_design.md#L111-L118), [455](../part3_design.md#L455)
*   **Discrepancy Description:** The inputs are `.txt.gz`, `read_edge_list` opens a plain `std::ifstream`, and the design never says how they are decompressed; `harness_weighted.py` already has `find_input`, `plain_text`, `vertex_count`, `select_rows`, `check_setup`, `append_csv`.
*   **Rationale:** A gap that blocks the first run; duplicated harness code.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Verified (`harness_weighted.py:122`, `plain_text`).
*   ***Proposed Edit:***
    *   Location: data-flow diagram, §10 harness bullet.
    *   Change: the diagram gains "harness: decompress once per graph (`plain_text`)" before the binary; §10: "`harness_part3.py` imports `find_input`, `plain_text`, `vertex_count`, `select_rows`, `check_setup` and `append_csv` from `harness_weighted.py` (importing does not change Part 2); `fw_bytes_needed` follows `matrix_bytes_needed`".

### [Issue 17] [`egm-comprehension`] - Severity: Major
*   **Sources:** Stage 6 M5.
*   **Affected Scope:** [Lines 135-142](../part3_design.md#L135-L142) (Glossary)
*   **Discrepancy Description:** "Step", the unit of the time budget, is never defined.
*   **Rationale:** Means, sample counts and statuses depend on it.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid; a comprehension gap is not contested.
*   ***Proposed Edit:***
    *   Location: Glossary.
    *   Change: "**Step:** one timed quantity in one launch of the binary: (graph, mode, algorithm or variant). Its repeated runs share one budget. The `reverse` and `bellman_ford_pass` rows of one `bf` run belong to the same step; the reported Bellman-Ford time is their sum, with both parts shown." (also answers design-1-13).

### [Issue 18] [`egm-comprehension`] - Severity: Major
*   **Sources:** Stage 6 N7 (raised here because it carries the push from Issue 2).
*   **Affected Scope:** [Lines 457](../part3_design.md#L457) (§10 Azure bullet)
*   **Discrepancy Description:** The Azure workflow assumes `azure_part2.sh` is known: no order of subcommands, no delete step, no push.
*   **Rationale:** The run cannot be followed from this document.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: §10 Azure bullet.
    *   Change: "Sequence: owner approves `git push origin feature/part3` → `check` → `create` → `setup` (`make test`, `make benchmark-part3`, `make test-part3` on Linux) → `upload` → `calibrate` (prints the VM-time estimate; owner approves) → `start` (re-arms auto-shutdown) → `status` (progress, ETA) → `fetch` → `delete`."

### [Issue 19] [`egm-comprehension`] - Severity: Major
*   **Sources:** Stage 6 M7.
*   **Affected Scope:** [Line 181](../part3_design.md#L181) (§2)
*   **Discrepancy Description:** `edge_list` holds `input_edge_t<W>`, the algorithms take `std::span<const weighted_edge<…>>`, and their compatibility is not stated.
*   **Rationale:** A reader cannot tell if a conversion is needed.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Verified in `include/graph/storage/input_edge.hpp`: `input_edge_t<weighted<X>>` is `weighted_edge<X>`.
*   ***Proposed Edit:***
    *   Location: end of §2.
    *   Change: "For a weighted policy `input_edge_t<weighted<X>>` is `weighted_edge<X>` (`storage/input_edge.hpp`), so the vector converts to the span without copying."

### [Issue 20] [`egm-design`] - Severity: Nit
*   **Sources:** design-1-9.
*   **Affected Scope:** [Lines 150-153](../part3_design.md#L150-L153), [418](../part3_design.md#L418)
*   **Discrepancy Description:** `read_edge_list(path, io::fixed_point_weight<double>{…})` cannot deduce W.
*   **Rationale:** The written call does not compile.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid: W appears only in the return type.
*   ***Proposed Edit:***
    *   Location: line 418.
    *   Change: `read_edge_list<gl::weighted<double>>(path, io::fixed_point_weight<double>{weight_decimals})`.

### [Issue 21] [`egm-design`, `egm-comprehension`] - Severity: Nit
*   **Sources:** design-1-14, design-1-16, Stage 6 N4 and N5.
*   **Affected Scope:** [Lines 91](../part3_design.md#L91), [423](../part3_design.md#L423), [507](../part3_design.md#L507)
*   **Discrepancy Description:** The all-pairs formula omits the reversal; the Floyd-Warshall side on W_3 to W_5 is unstated; Alternative F says "3 × 10" passes where one pass answers all three sources.
*   **Rationale:** Inaccurate figures in the comparison the report must justify.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: CUJ 4, §8 `allpairs`, Alternative F.
    *   Change: formula "one reversal + n × mean pass time (spread reported)"; on W_3 to W_5 the Floyd-Warshall side is the c·n³ fit, `extrapolated`, with the `no_memory` caveat; Alternative F: "repeated 10 times in case study 1 and n times in all-pairs".

### [Issue 22] [`egm-design`] - Severity: Nit
*   **Sources:** design-1-10.
*   **Affected Scope:** [Lines 64](../part3_design.md#L64), [165-167](../part3_design.md#L165-L167)
*   **Discrepancy Description:** The exactness bound covers single weights, not path sums.
*   **Rationale:** Exact comparison needs every sum to stay below 2^53.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid and the numbers are easy: scaled |w| ≤ 1,000 and paths have at most n − 1 arcs, so sums stay below 10^10 on W_5, far below 2^53 ≈ 9 × 10^15.
*   ***Proposed Edit:***
    *   Location: after the §1 parser block.
    *   Change: "Sums stay exact while (n − 1) × max |scaled w| ≤ 2^53; on the case-study graphs that is at most 10^10. Floyd-Warshall cells affected by a negative cycle can grow past it; they are reported as `negative_cycle`, never printed."

### [Issue 23] [`egm-design`] - Severity: Nit
*   **Sources:** design-1-11.
*   **Affected Scope:** [Lines 265-267](../part3_design.md#L265-L267)
*   **Discrepancy Description:** "{target} when v == target" and "empty when dist is ±inf" conflict when a cycle runs through the target.
*   **Rationale:** Order of rules undefined.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: `path_to_target` comment.
    *   Change: "Empty when dist[v] is +inf or −inf (checked first, so a target on a negative cycle gives empty); otherwise {target} when v == target; …".

### [Issue 24] [`egm-zoom-out`, `egm-design`, `egm-comprehension`] - Severity: Nit
*   **Sources:** Stage 5 N2, design-1-15, Stage 6 N6.
*   **Affected Scope:** [Lines 103](../part3_design.md#L103), [424](../part3_design.md#L424)
*   **Discrepancy Description:** Doubling sizes can overshoot the budget by about 8×; calibration "decides" what the run-time rule decides.
*   **Rationale:** Unbounded calibration; two deciders.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: CUJ 6, §8 `calibrate`.
    *   Change: "the next size runs only if 8 × the last run's time fits the budget"; "calibration only estimates (VM time, W_3 to W_5); the run-time budget rule decides repetitions"; the Bellman-Ford calibration pass reuses the `bf` answer run when one exists.

### [Issue 25] [`egm-zoom-out`] - Severity: Nit
*   **Sources:** Stage 5 N4, structural-1-11.
*   **Affected Scope:** [Lines 263](../part3_design.md#L263), [324](../part3_design.md#L324), [429](../part3_design.md#L429)
*   **Discrepancy Description:** `find_negative_cycle` takes options no caller changes; `bellman_ford` ends in `/* same tail */`; the `info` metric keeps the old field name.
*   **Rationale:** Small API gaps.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: §§4, 5, 8.
    *   Change: `find_negative_cycle(g, negative_loops = {}, stats = nullptr)`; `bellman_ford` signature written in full (`const G& g, vertex_id target, span negative_loops = {}, bellman_ford_options options = {}, bellman_ford_stats* stats = nullptr`, no extra constraint); metric `negative_self_loops` defined as "the size of `negative_self_loop_edges`".

### [Issue 26] [`egm-comprehension`, `egm-structural`] - Severity: Nit
*   **Sources:** Stage 6 N1, N2, N3, N8; structural-1-7.
*   **Affected Scope:** [Lines 38-47](../part3_design.md#L38-L47), [141-329](../part3_design.md#L141-L329) (headings)
*   **Discrepancy Description:** Review codes in headings unexplained; graph sizes only implied; case studies unnumbered; Goal 7 says Linux per commit, the plan says macOS per commit and Linux on the VM.
*   **Rationale:** Self-containment.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid and cheap.
*   ***Proposed Edit:***
    *   Location: Overview, Goals 6 and 7, Technical Plan intro.
    *   Change: one line under Technical Plan: "Codes such as B1 or M6 refer to the review findings in [part3.md](part3.md)". A table in Overview: W_1 25,000 / 549,953 arcs / 4,998 negative; W_2 25,000 / 824,964 / 0; W_3 100,000 / 6,099,941 / 0; W_4 1,000,000 / 30,999,978 / 0; W_5 10,000,000 / 39,999,995 / 0. Goal 6 numbers the case studies 1 to 4. Goal 7: "on macOS after every commit; on Linux on the VM before any measurement".

### [Issue 27] [`egm-style`] - Severity: Nit
*   **Sources:** style-1-8 to style-1-15, style-1-17, style-1-20, structural-1-9.
*   **Affected Scope:** lines 29, 30, 34, 36, 79, 97, 104, 133, 156, 345, 532, 550, 594
*   **Discrepancy Description:** Wording: hyperbole ("no algorithm that answers anything"), "naively", unbased "weeks / days to months", filler lead-in, redundant history, "estimated hours", "a copy of", "existing tests prove it", past tense, unquantified "much faster" / "often fewer", CUJ 6's extra Budget field, the S2 label.
*   **Rationale:** Precision.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Cheap; each verified (the io tests do assert messages, `test_edge_list.cpp:190`).
*   ***Proposed Edit:***
    *   Location: as listed.
    *   Change: "no shortest-path algorithm accepts it"; "if reused unchanged"; "estimated from n³ and n·m, not measured"; "Proposed solution:"; drop the redundant sentence at 97; "(estimated 1.5 to 8 h)"; "adapted from"; "the existing io tests, which assert the messages, must still pass"; "Goes in its own commit"; quantify or drop at 532 and 550; CUJ 6's Budget field moves to the Risks section; Alternatives relabelled A to W in order (S2 becomes T).

### [Issue 28] [`egm-structural`] - Severity: Nit
*   **Sources:** structural-1-8, structural-1-10, style-1-21.
*   **Affected Scope:** whole range
*   **Discrepancy Description:** bullet spacing, rule and numbering consistency, GiB vs GB.
*   ***AI Suggestion:*** DECLINE
*   ***Justification:*** Cosmetic; GiB at line 615 is the accurate unit for the Mac's RAM (16 GiB).

### [Issue 29] [`egm-zoom-out`] - Severity: Nit
*   **Sources:** Stage 5 N5, N6.
*   **Affected Scope:** [Lines 158-174](../part3_design.md#L158-L174), [457](../part3_design.md#L457)
*   **Discrepancy Description:** `fixed_point_weight` more generic than its one user; `azure_part3.sh` is a copy rather than parameters on `azure_part2.sh`.
*   ***AI Suggestion:*** DECLINE
*   ***Justification:*** Both are owner decisions recorded in the document (generic parser in the library; Part 2 programs untouched, Non-Goals).

### [Issue 30] [`egm-zoom-out`, `egm-inventive-catalyst`] - Severity: Nit
*   **Sources:** Stage 5 N7, N8, N9.
*   **Affected Scope:** §§3, 4, 5, 6, 7
*   **Discrepancy Description:** two entry points plus `reversed_graph`; the no-copy view; earlier detection in `find_negative_cycle`.
*   ***AI Suggestion:*** DECLINE
*   ***Justification:*** N7 and N8 are owner decisions (brief B4 and decision f) that the reviewers themselves accept. N9 speeds up a check that is O(1) on four of the five graphs.

## Deferred to Stage 9 (Detailed Implementation Plan, line 629 on)

* Stage 5 M9: the repetition-budget rule tested in the binary end to end, not in Python.
* Stage 6 N9: test descriptions that omit n or the example's source (line 758).
* Stage 6 N10: stale plan item 3; `make test-part3` scheduled one commit early.
* The plan follow-ups listed under Issues 1, 3, 4, 5 and 10.
