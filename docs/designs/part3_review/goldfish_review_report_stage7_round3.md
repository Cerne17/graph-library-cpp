# Goldfish Annotated Review Report

**Status:** resolved (2026-11-22): 21 PROPOSE FIX items applied to `part3_design.md` lines 1-768 (35 replacements, all matched); 3 declined.

**Target:** `docs/designs/part3_design.md` (lines 1-734, FeatureRange)

**Setup:** Stage 7 round 3, findings from [critic_report_round3.md](critic_report_round3.md) (0 blockers, 5 major, 19 nits). Earlier rounds: [round 1](goldfish_review_report_round1.md), [round 2](goldfish_review_report_round2.md) (renamed from `goldfish_review_report.md`; the design's decision-log link is updated in Issue 13).

**Counts:** the 24 findings are grouped into Issues 1-23 (Issue 5 merges style-3-3 and structural-3-1); Issue 24 is the declined half of design-3-6, whose other half is Issue 8. 21 PROPOSE FIX, 0 CONTEST, 0 ESCALATE, 3 DECLINE.

## Prioritized Findings & AI Suggestions

### [Issue 1] [`egm-design`] - Severity: Major
*   **Affected Scope:** [Lines 478, 511, 520, 715](../part3_design.md#L511-L520) (§8 `bf`; §9; §10 harness; Risks)
*   **Discrepancy Description:** design-3-1. The harness's cross-check `bf` launches share the `(graph, bf)` key with the case-study launch: restart can confuse them, their times mix into the case-study 3 mean, `--sources` is not set to the set (40 and 50 print nothing), and the default budget rule repeats each check up to 10 times (about 50 extra VM-minutes on W_1 with a cycle).
*   **Rationale:** Corrupted statistics and restart state, an incomplete check, wasted VM time.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid; the round 2 fix (Issue 4) moved the check into the harness without giving it its own key or options.
*   ***Proposed Edit:***
    *   Location: §10 harness requirements (now a sub-list, Issue 5); §8 `bf`.
    *   Change: cross-check launches are `bf --target t --sources <whole set> --runs 1`, recorded under mode `bf_check` with the target in the key (`(graph, bf_check, t)`); their rows never enter `mean_s` / `median_s`; completion and attempts are tracked per key. §8 `bf`: "`--runs 1` gives a single untimed-for-statistics answer run."

### [Issue 2] [`egm-design`] - Severity: Major
*   **Affected Scope:** [Lines 225-229, 252-256, 292-305, 563-567](../part3_design.md#L292-L305) (§3; §4; Alternative C)
*   **Discrepancy Description:** design-3-2. `reversed_graph<G>` is directed and satisfies `weighted_graph`, so `bellman_ford(reversed(g), t)` and `reversed(reversed(g))` compile and return distances from t labelled as distances to t.
*   **Rationale:** The silent wrong answer the type exists to prevent.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Verified against §3 and §4: `reversed` requires only `G::direction_type::is_directed`, and `bellman_ford` reverses any directed G.
*   ***Proposed Edit:***
    *   Location: §3 signatures and tests; §4 `bellman_ford` signature.
    *   Change: a trait `is_reversed_graph<G>` in `storage/reversed.hpp`; `reversed` and `bellman_ford` require `!is_reversed_graph<G>`. Tests: local concepts show `reversed(reversed(g))` and `bellman_ford(reversed(g), t)` are rejected; `bellman_ford_on_reversed(reversed(g), t)` stays the one way to pass a reversal.

### [Issue 3] [`egm-style`] - Severity: Major
*   **Affected Scope:** [Lines 42, 670](../part3_design.md#L42) (The Problem; Alternative S), against line 676
*   **Discrepancy Description:** style-3-1. "Days to months" for all-pairs on W_3 to W_5 is far too low; the design's own per-pass figures give months on W_4 and years on W_5.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** n × the estimated pass time: W_3, 10⁵ × 0.5 to 2 s ≈ 0.6 to 2.3 days; W_4, 10⁶ × 5 to 20 s ≈ 58 to 230 days; W_5, 10⁷ × 20 to 100 s ≈ 6 to 30 years.
*   ***Proposed Edit:***
    *   Location: The Problem; Alternative S.
    *   Change: "All-pairs Bellman-Ford would take about 1 to 2 days on W_3, 2 to 8 months on W_4 and 6 to 30 years on W_5 (n × an estimated pass of 0.5-2 s, 5-20 s and 20-100 s)"; Alternative S: "months on W_4 and years on W_5".

### [Issue 4] [`egm-style`, `egm-design`, `egm-structural`] - Severity: Major
*   **Affected Scope:** [Line 719](../part3_design.md#L719) (Risks), against §7
*   **Discrepancy Description:** style-3-2 (also design-3-3, structural-3-3). The risk row's trigger cites a `tree_from` assertion that §7 no longer defines.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid; round 2 removed the row-wide assertion.
*   ***Proposed Edit:***
    *   Location: Risks, path-walk row.
    *   Change: trigger "a caller reconstructs a path for a pair with `minus_infinity(i, j)` (or a −∞ vertex), and release builds do not check this"; mitigation unchanged.

### [Issue 5] [`egm-style`, `egm-structural`] - Severity: Major
*   **Affected Scope:** [Lines 519-536](../part3_design.md#L519-L536) (§10)
*   **Discrepancy Description:** style-3-3 and structural-3-1. Lines 520 and 535 pack many requirements into single sentences; the metric table breaks the bullet list.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid; these lines are what the harness tests are written from.
*   ***Proposed Edit:***
    *   Location: §10.
    *   Change: the harness bullet becomes a sub-list (imports; own `check_setup`; CLI; decompression; memory check; streaming and attempt ids; completion and restart; statistics; estimates; cross-check (Issue 1); `--no-bf-pass`; seconds only in the report). The Azure bullet splits into "VM size", "Sequence" and "Auto-shutdown". The metric table moves after the list under its own lead-in line.

### [Issue 6] [`egm-structural`, `egm-design`, `egm-style`] - Severity: Nit
*   **Affected Scope:** [Line 486](../part3_design.md#L486) (§8 `info` metrics)
*   **Discrepancy Description:** structural-3-2 (also design-3-4, style-3-4). `max_abs_weight` is missing from the `info` list.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** add `max_abs_weight` and, for `fw`, `memory_bytes` (`all_pairs::bytes()`).

### [Issue 7] [`egm-design`] - Severity: Nit
*   **Affected Scope:** [Lines 383-384, 422, 437-447](../part3_design.md#L437-L447) (§6 comment; §7)
*   **Discrepancy Description:** design-3-5. When dist(i, i) < 0, `pred(i, i)` is overwritten, breaking `parent[source] == 0`.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Valid; resetting the diagonal after the run is O(n) and keeps the documented invariant.
*   ***Proposed Edit:*** algorithm adds "after the loop: pred(i, i) = 0 for every i".

### [Issue 8] [`egm-design`] - Severity: Nit
*   **Affected Scope:** [Lines 52, 317-336, 477](../part3_design.md#L52) (Goal 3; §4; §8 `variants_agree`)
*   **Discrepancy Description:** design-3-6. Equal-weight ties can give different `next` across variants, while Goal 3 says "the same answers".
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** Goal 3 and `variants_agree`: "the same distances (and, with synchronous rounds, the same number of rounds for stop_when_stable and frontier); `next` may differ between paths of equal weight".

### [Issue 9] [`egm-design`] - Severity: Nit
*   **Affected Scope:** [Lines 481, 521](../part3_design.md#L481) (§8 `calibrate`, `fw`; §10 watchdog)
*   **Discrepancy Description:** design-3-7. The 600 s watchdog floor equals the budget: a calibration run admitted at "8 × last ≤ budget" or a slow first progress interval can be killed.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** watchdog floor max(2 × `--budget-seconds`, 5 × longest gap); `fw` and `calibrate` also print a `progress` row at least every 60 s.

### [Issue 10] [`egm-design`] - Severity: Nit
*   **Affected Scope:** [Line 228](../part3_design.md#L228) (§3 comment)
*   **Discrepancy Description:** design-3-8. `from_edges` sorts each block, so the cost is O(n + m log d_max), not O(n + m).
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** Verified in `storage/csr.hpp`, step 4 (sort per block).
*   ***Proposed Edit:*** "O(n + m log d_max) time (the per-vertex sort in `from_edges`)"; same correction in §4's cost line.

### [Issue 11] [`egm-structural`] - Severity: Nit
*   **Affected Scope:** [Line 427](../part3_design.md#L427) (§7 signature)
*   **Discrepancy Description:** structural-3-4. `no_op_pivot` is never declared.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** `struct no_op_pivot { void operator()(vertex_id, vertex_id) const noexcept {} };`

### [Issue 12] [`egm-structural`] - Severity: Nit
*   **Affected Scope:** headings at lines 175, 216, 220, 258, 347, 378, 396
*   **Discrepancy Description:** structural-3-5. Parentheses mix review codes with "io" and "owner-approved".
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** parentheses keep only review codes; "owner-approved" moves into each section's first sentence.

### [Issue 13] [`egm-structural`] - Severity: Nit
*   **Affected Scope:** [Lines 87-88](../part3_design.md#L87-L88) (Decision log)
*   **Discrepancy Description:** structural-3-6. Both links say "report".
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** "round 1 resolution report" → `part3_review/goldfish_review_report_round1.md`; "round 2 resolution report" → `part3_review/goldfish_review_report_round2.md`; add a round 3 entry.

### [Issue 14] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Line 586](../part3_design.md#L586) (Alternative F)
*   **Discrepancy Description:** style-3-5. "a Θ(n·m) pass" should be a worst case.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** "an O(n·m) worst-case pass".

### [Issue 15] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Lines 50, 52](../part3_design.md#L50-L52) (Goals 1, 3)
*   **Discrepancy Description:** style-3-6. The commit reference sits after the not-yet-existing `--directed` program; "on every graph" claims more than tests show.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** move "(tests already committed in `879a495`)" right after the hand-checked tests sentence; "on every test graph".

### [Issue 16] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Line 214](../part3_design.md#L214) (§1 exactness)
*   **Discrepancy Description:** style-3-7. "Sums stay below 10^10" is loose for W_1 and W_2.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Justification:*** For n = 25,000 the bound is about 2.5 × 10⁷; 10¹⁰ is the bound for W_5 (n = 10⁷) if its weights are in the same range.
*   ***Proposed Edit:*** "sums stay below 2.5 × 10⁷ on W_1 and W_2 (and below 10¹⁰ on W_5 if its weights are in the same range)".

### [Issue 17] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Line 455](../part3_design.md#L455) (§7 paragraph)
*   **Discrepancy Description:** style-3-8. One paragraph covers five topics.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** split into "Memory optimization", "Correctness with negative cycles" (including row k and NaN) and "Memory footprint".

### [Issue 18] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Line 453](../part3_design.md#L453) (§7 implementation note)
*   **Discrepancy Description:** style-3-9. No row or test file named for the loop timings and the bit-identity check.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** "timed by `calibrate` as `calib,floyd_warshall_branchless,…` and `calib,floyd_warshall_branchy,…` rows and shown in the report's method section; bit identity checked in `tests/test_floyd_warshall_differential.cpp`".

### [Issue 19] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Lines 500, 509](../part3_design.md#L500-L509) (§9)
*   **Discrepancy Description:** style-3-12. "that would fit" is an unclear modifier.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** "`skipped` applies only to a step that would fit in memory but is turned off in the config or uses the matrix storage"; the table row says "not run by design (see above)".

### [Issue 20] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Line 705](../part3_design.md#L705) (Alternative Y)
*   **Discrepancy Description:** style-3-13. "Much less memory traffic" is not quantified.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** "about B times less memory traffic per pivot for B × B tiles".

### [Issue 21] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Line 555](../part3_design.md#L555) (Alternative A)
*   **Discrepancy Description:** style-3-14. "the owner's `storage/matrix.hpp`" reads oddly.
*   ***AI Suggestion:*** PROPOSE FIX
*   ***Proposed Edit:*** "`storage/matrix.hpp`, which AGENTS.md reserves for the project owner".

### [Issue 22] [`egm-style`] - Severity: Nit
*   **Affected Scope:** [Lines 547-707](../part3_design.md#L547-L707) (Alternatives)
*   **Discrepancy Description:** style-3-10. Every entry repeats "Rejected by author".
*   ***AI Suggestion:*** DECLINE
*   ***Justification:*** The prefix is the EGM design-document template's required form for each "Reason for Rejection" (`egm-draft` `references/design_doc.md`), which later EGM steps read; keeping it costs a few words.

### [Issue 23] [`egm-style`] - Severity: Nit
*   **Affected Scope:** lines 42, 94, 108, 132, 481, 716
*   **Discrepancy Description:** style-3-11. The runtime estimate and "calibration only estimates" appear in several places.
*   ***AI Suggestion:*** DECLINE
*   ***Justification:*** Round 2 already put the full statement and basis once in The Problem; the other places are short references a reader of that section needs.

### [Issue 24] [`egm-design`] - Severity: Nit
*   **Affected Scope:** §4 frontier order
*   **Discrepancy Description:** part of design-3-6: sort the frontier by id so `next` is identical across variants.
*   ***AI Suggestion:*** DECLINE
*   ***Justification:*** Issue 8 narrows the promise to distances and rounds instead, which avoids an O(f log f) sort per round for no case-study benefit.
