# Goldfish Annotated Review Report

**Status:** resolved (2026-11-22): 27 PROPOSE FIX items and the 2 follow-on edits applied to `part3_design.md` lines 1-734; 1 contested (Issue 13), no change.

**Target:** `docs/designs/part3_design.md` (lines 1-699, FeatureRange)

**Setup:** Stage 7 round 2, findings from [critic_report_round2.md](critic_report_round2.md) (0 blockers, 9 major, 21 nits). Round 1 is kept in [goldfish_review_report_round1.md](goldfish_review_report_round1.md) and [critic_report_round1.md](critic_report_round1.md).

**Counts:** the 30 findings are grouped into Issues 1-28 (Issues 21 and 26 each merge two findings): 27 PROPOSE FIX, 1 CONTEST, 0 ESCALATE, 0 DECLINE. Issues 29 and 30 are not reviewer findings: they are follow-on edits that keep other sections consistent with Issues 2, 3, 4 and 9. Three fixes choose between options the reviewers offered (Issues 1, 4 and 8); the choice and the rejected option are stated so the owner can overrule them.

## Prioritized Findings & AI Suggestions

### [Issue 1] [`egm-design`, `egm-style`] - Severity: Major
*   **Affected Scope:** [Lines 50, 307-325](../part3_design.md#L307-L325) (Goal 3; §4 round pseudocode)
*   **Discrepancy Description:** design-2-1 (merged style-2-2). With in-place updates, a full scan in id order can settle a chain in one round that the frontier variant needs n rounds for, so "each later variant does no more work" is false for frontier against stop_when_stable; and "no more work" (≤) would pass with no pruning at all.
*   **Rationale:** The goal and the report's comparison would rest on a false claim.
*   ***AI Suggestion:*** PROPOSE FIX (option 1 of the two offered: synchronous rounds)
*   ***Justification:*** Valid. With synchronous rounds (each round reads the distances of the previous round), stop_when_stable and frontier compute the same distances in the same rounds, and frontier relaxes a subset of the arcs, so textbook ≥ stop_when_stable ≥ frontier holds in both counters and is provable. The other option (weaken the goal) keeps a claim the report could not make. Cost: one extra array of n distances; after each round, only the changed entries are copied (O(changed)). The detection rule (a change in round n) and the −∞ argument are the textbook ones for synchronous rounds. The star fixture's counts (9,900 / 198 / 99) stay the same.
*   ***Proposed Edit:***
    *   Location: Goal 3; the sentence before the pseudocode; the pseudocode; Alternative M's reason.
    *   Change: Goal 3: "… Tests show, through counters of rounds and relaxations, that textbook ≥ stop_when_stable ≥ frontier on every graph, strictly on a star fixture, with the same answers." Before the pseudocode: "Rounds are synchronous: each round reads `prev`, the distances at the end of the previous round, and writes `dist`; after the round, the changed entries are copied into `prev`." Pseudocode relaxes with `prev[x] + w < dist[u]` and `prev` is synced from `changed_this_round`; written as `if variant == frontier: frontier = [target] else: scan every x with prev[x] < +inf` (also Issue 23).

### [Issue 2] [`egm-design`, `egm-style`] - Severity: Major
*   **Affected Scope:** [Lines 42, 84, 98, 440, 442, 641, 647](../part3_design.md#L42) (Problem; Decisions; CUJ 2; §7; Alternatives)
*   **Discrepancy Description:** design-2-2 (merged style-2-4). Each pivot streams about 24·n² bytes (about 15 GB at n = 25,000), about 375 TB per run; at 10-20 GB/s that is about 5-10 h, so the 1.5 h lower bound is too low. The masked select writes every cell on every pivot. Tiled Floyd-Warshall is missing from the alternatives, and the throughput assumption is not stated.
*   **Rationale:** The VM budget, the 22 h split and the ETA all depend on this estimate.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid arithmetic: 25,000² × 24 B ≈ 1.5 × 10¹⁰ B per pivot × 25,000 pivots ≈ 3.75 × 10¹⁴ B. Tiling is a real option; it is rejected below for this project, with a reason the owner can overrule.
*   ***Proposed Edit:***
    *   Location: The Problem; Decision log / Open questions; CUJ 2; §7 implementation note; new Alternative Y.
    *   Change: The Problem states the basis once: "about 24·n² bytes of memory traffic per pivot; at an assumed 10-20 GB/s of single-core bandwidth, 5 to 10 h per run at n = 25,000; at the compute bound (1-2 ns per cell) 4 to 9 h"; other places say "estimated 5 to 10 h (see The Problem)". §7: "Before choosing, the branchless and the branchy j loop are timed at n = 4,000 (192 MB, larger than any cache) on the VM; the faster one is used and both times are reported." Calibration also measures STREAM-like bandwidth once and prints the bandwidth bound next to the c·n³ fit. New Alternative Y "Tiled (blocked) in-place Floyd-Warshall": pros, less memory traffic with the same single-matrix memory; cons, a different loop structure from the class algorithm, harder to combine with the row skip and the predecessor matrix, more code to test; rejected by author for this project, with the progress ETA as the guard if the run is slow.

### [Issue 3] [`egm-design`] - Severity: Major
*   **Affected Scope:** [Lines 98, 409, 442](../part3_design.md#L409) (CUJ 2; `all_pairs::tree_from`; §7)
*   **Discrepancy Description:** design-2-3. The whole-row precondition fires on valid `ok` pairs in a row that has `minus_infinity` cells elsewhere.
*   **Rationale:** Debug tests on graphs with negative cycles would abort on correct calls.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid: the §7 argument needs only the requested cell to be unaffected, since every vertex on its predecessor chain is then unaffected too.
*   ***Proposed Edit:***
    *   Location: `tree_from` comment; §7 paragraph.
    *   Change: `row_tree tree_from(vertex_id i) const&; // no copy; reconstruct_path(tree_from(i), j) is valid when !minus_infinity(i, j)`; no row-wide assertion. §7: "The program reconstructs a path only for `ok` pairs."

### [Issue 4] [`egm-design`] - Severity: Major
*   **Affected Scope:** [Lines 55, 162, 463-464, 504, 680](../part3_design.md#L463-L464) (Goal 8(d); Glossary; §8 `fw`; §10; Risks)
*   **Discrepancy Description:** design-2-4. The restart unit is the launch, so a crash during the `fw` cross-checks throws away the finished Floyd-Warshall run.
*   **Rationale:** Goal 8(d) understates the loss.
*   ***AI Suggestion:*** PROPOSE FIX (option 2: move the cross-check out of the `fw` launch)
*   ***Justification:*** Valid. Moving the check removes the problem instead of documenting it: the harness launches `bf` toward each vertex of the set (five short launches) and compares their printed distances and statuses with the `fw` path rows. The comparison stays exact because scaled sums are exact and both print 2 decimals from the same integers. An `fw` launch then holds only the Floyd-Warshall run and its path rows, so a crash loses at most that run, which no restart scheme can save without checkpoints.
*   ***Proposed Edit:***
    *   Location: Goal 8(d); §8 `fw`; §9 `mismatch`; §10 harness; Risks row 1.
    *   Change: Goal 8(d): "A run that dies loses at most its unfinished launch; the only multi-hour launch is a single Floyd-Warshall run." §8 `fw`: drop the untimed Bellman-Ford passes and the `check,fw_vs_bf` row. §10: "After an `fw` launch, the harness runs `bf --target t` for each t in the set and writes a `fw_vs_bf` check (`ok` / `mismatch`) into `results_part3.csv`." §9 `mismatch` emitted in: `stdout: check` (variants) and `results_part3.csv` (`fw_vs_bf`).

### [Issue 5] [`egm-design`] - Severity: Major
*   **Affected Scope:** [Line 504](../part3_design.md#L504) (§10 harness)
*   **Discrepancy Description:** design-2-5. `check_setup` requires `data/baseline.txt`, points to `make benchmark-weighted` and reads `cfg["labels"]`.
*   **Rationale:** Reusing it raises `KeyError` on the Part 3 config.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Verified in `analysis/harness_weighted.py:153-169`.
*   ***Proposed Edit:***
    *   Location: §10 import list.
    *   Change: import `find_input`, `plain_text`, `vertex_count`, `select_rows`, `append_csv`; `harness_part3.py` has its own `check_setup` (binary built by `make benchmark-part3`, every config graph present in `data/part3/`, `directed` set on every row).

### [Issue 6] [`egm-structural`] - Severity: Major
*   **Affected Scope:** [Lines 122, 467, 470-480](../part3_design.md#L470-L480) (§8 row table)
*   **Discrepancy Description:** structural-2-1. No row kind for calibration sizes, the c·n³ fit, or the target of an `allpairs` pass.
*   **Rationale:** The binary and the harness would each guess the format.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: §8 row table.
    *   Change: add `calib,floyd_warshall,<n>,<arcs>,<run>,<micros>` (one per calibration run), `fit,<model>,<c>,<points>` (`model` in `floyd_warshall_n3`, `bandwidth_bytes_per_s`), and an optional last column on `time`: `time,<algorithm>,<run>,<micros>[,<param>]`, where `param` is the target for `allpairs_pass` and empty otherwise.

### [Issue 7] [`egm-structural`] - Severity: Major
*   **Affected Scope:** [Line 506](../part3_design.md#L506) (§10 CSV schemas)
*   **Discrepancy Description:** structural-2-2. No list of `metric` names in `results_part3.csv`.
*   **Rationale:** The report script and the restart logic read this file.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: after the `results_part3.csv` schema.
    *   Change: a table of metrics: `mean_s`, `median_s` (with `samples` and `ok` / `fewer_runs`); `reverse_mean_s`; `estimate_s` (`status=extrapolated`, formula in `note`); `memory_bytes` (`no_memory`); `negative_cycle` (`yes`/`no`, weight and witness in `note`); `pass_spread_s`, `targets_measured` (allpairs); `rounds`, `relaxations` (per variant); `fw_vs_bf` (`ok` / `mismatch`); `max_abs_weight`. "Estimate rows" means rows with `status=extrapolated`.

### [Issue 8] [`egm-style`] - Severity: Major
*   **Affected Scope:** [Lines 42, 110, 363, 641](../part3_design.md#L110) (Problem; CUJ 4; §5; Alternative S)
*   **Discrepancy Description:** style-2-1. "Far over the step budget on every case-study graph" has no basis on W_1 and W_2, where n passes of about 20 ms would fit in 600 s.
*   **Rationale:** The claim decides that all-pairs is always extrapolated.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid: no per-pass time on W_1 or W_2 is known. Letting the budget rule decide is consistent with Goal 8 and may give a fully measured all-pairs time where it is cheap.
*   ***Proposed Edit:***
    *   Location: CUJ 4; §8 `allpairs`; The Problem; Alternative S; §5 last sentence.
    *   Change: `allpairs` times passes toward targets in a fixed-seed order; if, after the first passes, reversal + n × the longest pass fits the budget, it continues over all n targets and reports the measured total (`ok`); otherwise it stops at the budget or after k targets and reports "one reversal + n × mean pass time" as `extrapolated`. CUJ 4 and Alternative S say "on W_3 to W_5 (estimated over budget); on W_1 and W_2 the budget rule decides". Drops the fallback sentence of CUJ 4 (Issue 16). §5: "In a full all-pairs run (possible on W_1 and W_2) the check is redundant…".

### [Issue 9] [`egm-style`, `egm-design`] - Severity: Major
*   **Affected Scope:** [Lines 55, 96, 442, 507, 685, 694](../part3_design.md#L507) (Goal 8(c); CUJ 2; §7; §10; Risks; NFR)
*   **Discrepancy Description:** style-2-3 (merged part of design-2-12). The stated needs fit in 16 GB, yet 32 GB is called the smallest size that fits.
*   **Rationale:** The owner approves cost against this sizing.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid. Peak need: 7.5 GB of Floyd-Warshall matrices on W_1/W_2; on W_5 (no Floyd-Warshall) about 1 GB of CSR (Part 2 measured 992 MB) plus the 640 MB edge list while building, and the reversal; the decompressed files stay on disk. A 16 GB VM leaves about 6 GB of headroom. Single-threaded runs do not need 4 vCPUs.
*   ***Proposed Edit:***
    *   Location: as listed.
    *   Change: "a 16 GB VM (for example `Standard_E2as_v5`, 2 vCPUs), about 6 GB above the peak need of 7.5 GB plus the graph; 32 GB only if calibration shows memory pressure".

### [Issue 10] [`egm-design`, `egm-style`] - Severity: Nit
*   **Affected Scope:** [Line 204](../part3_design.md#L204) (§1 exactness)
*   **Discrepancy Description:** design-2-6 (merged style-2-15). "|scaled w| ≤ 1,000" has no source and is not enforced.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid; W_1 and W_2 were scanned (weights in [−5.00, 10.00]); W_3 to W_5 were not.
*   ***Proposed Edit:***
    *   Location: §1 last paragraph; §8 info metrics.
    *   Change: "(W_1 and W_2 were scanned: weights in [−5.00, 10.00]). The program prints `info,max_abs_weight` and exits with an error if (n − 1) × max |scaled w| > 2^53."

### [Issue 11] [`egm-design`] - Severity: Nit
*   **Affected Scope:** [Lines 204, 431-442](../part3_design.md#L431-L442) (§7)
*   **Discrepancy Description:** design-2-7. Overflow to −inf and NaN under negative cycles is not argued.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid and short.
*   ***Proposed Edit:***
    *   Location: §7 paragraph after the algorithm.
    *   Change: "Affected cells may reach −inf; then −inf + +inf gives NaN, but NaN < x is false, so NaN is never stored, and finiteness (which `minus_infinity` reads) is unchanged."

### [Issue 12] [`egm-design`, `egm-style`] - Severity: Nit
*   **Affected Scope:** [Line 440](../part3_design.md#L440) (§7 implementation note)
*   **Discrepancy Description:** design-2-8 (merged style-2-9). "Bit-identical" holds only when dist(k, k) ≥ 0; with i == k the hoisted row pointers alias; no baseline or test for the speed-up.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid. Skipping row i = k always is safe: if dist(k, k) ≥ 0 row k does not change in step k; if dist(k, k) < 0, the cells row k would update are already affected, and finiteness does not change.
*   ***Proposed Edit:***
    *   Location: §7 algorithm and implementation note.
    *   Change: the i loop skips i = k; "bit-identical to the plain loop on every cell not affected by a negative cycle, checked by a test on random graphs"; "speed-up measured against the plain loop at n = 4,000" (Issue 2).

### [Issue 13] [`egm-design`] - Severity: Nit
*   **Affected Scope:** [Lines 98, 407, 464](../part3_design.md#L407) (`minus_infinity`; §8 `fw`)
*   **Discrepancy Description:** design-2-9. Classifying a whole column costs n × |on_cycle|; proposes per-row and per-column "reaches a cycle" bitmaps for O(1) `minus_infinity`.
*   ***AI Suggestion:*** CONTEST
*   ***Justification:*** The proposed bitmaps give a wrong answer: i may reach a cycle C1 and j be reachable from a different cycle C2 with no walk i → C → j, and `row[i] && col[j]` would mark (i, j) as −∞. The §7 test (lines 431-442) needs the same k on both sides. After Issue 4 the binary no longer classifies columns: it calls `minus_infinity` only for the 20 requested pairs, which costs 20 × |on_cycle|.
*   ***Proposed Edit:*** none.

### [Issue 14] [`egm-design`, `egm-style`] - Severity: Nit
*   **Affected Scope:** [Line 467](../part3_design.md#L467) (§8 `calibrate`)
*   **Discrepancy Description:** design-2-10 (merged style-2-13). The binary cannot see whether a `bf` answer run is recorded.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: §8 `calibrate`; options block.
    *   Change: new option `--no-bf-pass`; "the harness passes it when a `bf` answer run is already recorded for the graph".

### [Issue 15] [`egm-design`] - Severity: Nit
*   **Affected Scope:** [Line 494](../part3_design.md#L494) (§9 `no_memory`)
*   **Discrepancy Description:** design-2-12. How "the VM's memory" is measured is not stated.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: `no_memory` row.
    *   Change: "… exceeds `MemAvailable` (from `/proc/meminfo` on Linux, `psutil.virtual_memory().available` elsewhere) minus a 1 GB margin".

### [Issue 16] [`egm-design`] - Severity: Nit
*   **Affected Scope:** [Line 110](../part3_design.md#L110) (CUJ 4)
*   **Discrepancy Description:** design-2-11. The fallback "if one pass alone exceeds the budget" cannot happen, since the first run always runs.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:*** dropped as part of Issue 8's CUJ 4 rewrite; "at least one pass is always measured".

### [Issue 17] [`egm-structural`, `egm-style`] - Severity: Nit
*   **Affected Scope:** [Lines 54, 507, 510-514](../part3_design.md#L510-L514) (Goal 7; §10; §11)
*   **Discrepancy Description:** structural-2-3 (merged part of style-2-12). `make benchmark-part3`, `make test-part3`, `--only` and `--calibrate-only` are used but not defined in range; Goal 7 omits the harness tests.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:***
    *   Location: §11, §10, Goal 7.
    *   Change: §11 "Makefile: `benchmark-part3` (release, `-DNDEBUG`) and `test-part3` (builds a checked binary, runs `tests/test_harness_part3.py`); `make test` unchanged." §10: "Harness CLI: `harness_part3.py [--only GRAPH] [--calibrate-only]`." Goal 7 adds "`make test-part3` passes after the commits that touch the program or the harness".

### [Issue 18] [`egm-structural`] - Severity: Nit
*   **Affected Scope:** [Lines 269, 454, 463](../part3_design.md#L454) (enum; CLI; rows)
*   **Discrepancy Description:** structural-2-4. CLI `stop` against enum `stop_when_stable`.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid.
*   ***Proposed Edit:*** CLI and row suffixes use the enum names: `textbook|stop_when_stable|frontier`.

### [Issue 19] [`egm-structural`, `egm-design`] - Severity: Nit
*   **Affected Scope:** [Lines 110, 456](../part3_design.md#L456) (options block; CUJ 4)
*   **Discrepancy Description:** structural-2-5. `--k` and `--seed` have no defaults.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** `--k N --seed S  allpairs: sample size (100) and seed (2025)`.

### [Issue 20] [`egm-structural`, `egm-style`, `egm-design`] - Severity: Nit
*   **Affected Scope:** [Lines 488, 491](../part3_design.md#L488-L491) (§9)
*   **Discrepancy Description:** structural-2-6. `ok` and `fewer_runs` hard-code 10.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** "with `--runs` runs (default 10)"; "1 to `--runs` − 1 measured runs".

### [Issue 21] [`egm-structural`, `egm-style`] - Severity: Nit
*   **Affected Scope:** [Lines 76-78, 122, 681](../part3_design.md#L76-L78) (Decision log; links)
*   **Discrepancy Description:** structural-2-7 (merged style-2-10) and style-2-11. Unlinked report and alternatives; the round 1 entry states no decision; the reconciliation entry packs seven decisions into one sentence.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** link `part3_review/goldfish_review_report_round1.md` and the alternatives/sections by anchor; split the reconciliation entry into sub-bullets; the round 1 entry lists what changed (self-loop seeds, per-pair −∞ in Floyd-Warshall, single witness, measured Floyd-Warshall time, progress rows and watchdog, status table, Risks section); add a round 2 entry the same way.

### [Issue 22] [`egm-structural`] - Severity: Nit
*   **Affected Scope:** [Lines 157-163](../part3_design.md#L157-L163) (Glossary)
*   **Discrepancy Description:** structural-2-8. "Step", "answer run" and "scaled weight" are used in Goals and CUJs before the Glossary.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** one line before Goal 1: "Terms such as *step*, *answer run* and *scaled weight* are defined in the [Glossary](#glossary)."

### [Issue 23] [`egm-structural`] - Severity: Nit
*   **Affected Scope:** [Lines 311, 314](../part3_design.md#L311-L314) (pseudocode)
*   **Discrepancy Description:** structural-2-10. The `|` in the pseudocode reads as an operator.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** written as `if variant == frontier: … else: …` (part of Issue 1).

### [Issue 24] [`egm-structural`] - Severity: Nit
*   **Affected Scope:** [Lines 564, 602, 628](../part3_design.md#L560-L628) (Alternatives G, M, Q)
*   **Discrepancy Description:** structural-2-9. Three reasons name the choice, not the trade-off.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** G: "a cycle is found only by running, and the tables must show it as a value"; M: "it has no rounds, so optimization 1 and the round-n rule would not apply"; Q: "rounding would return, and 3-decimal input would be accepted silently".

### [Issue 25] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Line 461](../part3_design.md#L461) (§8 budget rule; Glossary; Goal 8(a))
*   **Discrepancy Description:** style-2-5. "Counting the first run" is ambiguous, and Goal 8(a) fails whenever a first run alone exceeds the budget.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** "Run 1 always runs. Another run starts while runs < `--runs` and elapsed (including run 1) + the longest run so far ≤ `--budget-seconds`." Goal 8(a): "… within its time budget, except that run 1 always runs".

### [Issue 26] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Lines 52, 53](../part3_design.md#L52-L53) (Goals 5, 6)
*   **Discrepancy Description:** style-2-7, style-2-8. Goal 6 says "mean" where CUJ 1 says mean and median; "every pair of random test graphs" is ambiguous.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** "the mean and median time"; "on every vertex pair of the random test graphs".

### [Issue 27] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Lines 507, 680](../part3_design.md#L507) (§10; Risks)
*   **Discrepancy Description:** style-2-12. When the run is split per graph is unclear.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** "If the estimate exceeds 22 h, the run is split per graph with `harness_part3.py --only <graph>`, re-arming the shutdown before each."

### [Issue 28] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Lines 152, 307, 608, 686, 695, 696](../part3_design.md#L608) (several)
*   **Discrepancy Description:** style-2-14. "(tables, s)"; "can only shorten"; "about 10¹² relaxations" (n·m = 6.1 × 10¹¹); "breaks AGENTS.md"; "none critical"; "each item".
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Each checked; 10⁵ × 6.1 × 10⁶ = 6.1 × 10¹¹.
*   ***Proposed Edit:*** "(tables, times in seconds)"; sentence replaced by Issue 1; "about 6 × 10¹¹ relaxations"; "violates the AGENTS.md rule that pushes need the owner's approval"; delete "none critical"; "each Implementation Plan item is its own commit".

### [Issue 29] [`egm-design`] - Severity: Nit
*   **Affected Scope:** [Line 408](../part3_design.md#L408) (`all_pairs::negative_cycle`)
*   **Discrepancy Description:** follows from Issue 3: the class comment for `tree_from` no longer mentions a row-wide check.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** covered by Issue 3's edit.

### [Issue 30] [`egm-design`] - Severity: Nit
*   **Affected Scope:** [Lines 676-690](../part3_design.md#L676-L690) (Risks)
*   **Discrepancy Description:** follows from Issues 2, 4 and 9: the risk rows give the old estimate (1.5 to 8 h), the old loss unit and the 32 GB size.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** risk rows updated to "5 to 10 h, bandwidth-bound", "loses at most one Floyd-Warshall launch" and "16 GB VM".
