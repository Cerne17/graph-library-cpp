# Goldfish Review Report: Stage 7 Critic Review, round 3

**Target:** `docs/designs/part3_design.md` (lines 1-734, the FeatureRange before "## Detailed Implementation Plan")

**Mode:** REVIEW. Standard for egm-design and egm-structural: `resources/document_standards/feature-spec-standard.md`. egm-style used its persona's rules only.

## Reviewer Status

| Reviewer | Status | Raw findings |
| --- | --- | --- |
| `egm-design` | completed | 2 Major, 6 Nit |
| `egm-structural` | completed | 6 Nit |
| `egm-style` | completed | 3 Major, 11 Nit |

Verdicts: egm-design says the design meets the standard. egm-structural passes it with Nits only. egm-style found no Blockers.

## Summary

The reviewers reported 28 raw findings. After deduplication there are **24 unique findings: 0 Blocker, 5 Major, 19 Nit.**

- Line 719 (the `tree_from` assertion) was raised by all three reviewers. It is merged into style-3-2. Severity is Major, following style; design and structural rated it Nit.
- The missing `max_abs_weight` at line 486 was raised by all three reviewers. It is merged into structural-3-2 as a Nit.

**Re-raised settled findings:** none. No reviewer raised a settled item again. design-3-2 is next to the settled item "two Bellman-Ford entry points plus reversed_graph", but it is a different issue: a type hole inside the chosen design, not a challenge to having two entry points. It is counted as new.

## Prioritized Findings (Blockers -> Major -> Nits)

### Blockers

None.

### Major

#### design-3-1. Cross-check `bf` launches share the `(graph, mode)` key with the case-study `bf` launch - Severity: Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** lines 478, 511, 520, 715
* **Discrepancy Description:**
  * After each `fw` launch, the harness runs `bf --target t` for each t in the cross-check set. These launches share the `(graph, bf)` key with the case-study launch toward target 100.
  * **Restart:** completion is keyed by a `done` row per `(graph, mode)`. So a restart can treat cross-checks that never ran as complete, or discard and rerun the wrong attempt.
  * **Statistics:** their `time,reverse` and `time,bellman_ford_pass` rows go into the same CSV. The case-study 3 Bellman-Ford mean and median would therefore mix in passes toward targets 10 to 50.
  * **Missing sources:** the design never says the cross-checks pass `--sources` equal to the set. With the default `10,20,30`, sources 40 and 50 print no rows, so `fw_vs_bf` cannot compare all 20 pairs.
  * **Cost:** each cross-check also follows the default budget rule (up to 10 runs, 600 s). Directed W_1 with a reachable negative cycle costs Θ(n·m) per pass, about 1.4 × 10^10 relaxations. That adds about 50 minutes of VM time for a check that needs one run.
* **Rationale & Recommendation:**
  * Key completion and attempts by (graph, mode, purpose or target), for example `bf:t=40` or a `check` role.
  * Leave cross-check rows out of the mean and median, through a tag column or a separate mode such as `bf_check`.
  * State that cross-checks run with `--runs 1 --sources <set>`.

#### design-3-2. The convenience `bellman_ford` and `reversed` accept a graph that is already reversed - Severity: Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** lines 225-229, 252-256, 292-305, 563-567
* **Discrepancy Description:**
  * `reversed_graph<G>` has `direction_type = directed` and satisfies `weighted_graph`.
  * So `bellman_ford(rev, t)` compiles, and the convenience overload reverses `rev` a second time. `reversed(rev)` also compiles.
  * Either way the result holds distances *from* t, labelled as distances *to* t. That is the silent wrong answer Alternative C exists to rule out.
* **Rationale & Recommendation:** the wrapper's purpose is that the wrong orientation does not compile. Constrain `bellman_ford` and `reversed` with `!is_reversed_graph<G>`, the counterpart of `reverse_input`. Add negative tests showing that `bellman_ford(reversed(g), t)` and `reversed(reversed(g))` are rejected.

#### style-3-1. The all-pairs Bellman-Ford estimate is far too low - Severity: Major
* **Reviewer Lens:** `egm-style` (CALIBRATE)
* **Affected Scope:** lines 42 and 670, against line 676
* **Discrepancy Description:**
  * Line 42 says all-pairs Bellman-Ford on W_3 to W_5 "would take days to months". Line 670 says "days or more".
  * The design's own per-pass figures (line 676) give much more:
    * W_4: about 58 to 230 days.
    * W_5: about 6 to 30 years.
  * The parenthetical at line 42 does not give the per-pass time the range rests on.
* **Rationale & Recommendation:** give one range per graph from the line 676 figures, for example "about a day on W_3, months on W_4, years on W_5 (n × the estimated pass time, Alternative T)". Use the same wording at line 670.

#### style-3-2. The risk table points to a `tree_from` assertion that §7 does not define - Severity: Major
* **Reviewer Lens:** `egm-style`. Also raised as design-3-3 and structural-3-3, both rated Nit.
* **Affected Scope:** line 719, against lines 411-424
* **Discrepancy Description:**
  * The trigger reads "`tree_from`'s assertion is compiled out in release".
  * §7 gives `tree_from` no assertion. It only says `reconstruct_path(tree_from(i), j)` is valid when `!minus_infinity(i, j)`.
  * The only related assertion is `reconstruct_path`'s range check on the target (path.hpp:22-26).
* **Rationale & Recommendation:** a reader cannot find the safeguard this risk row relies on. Choose one fix:
  * State the precondition and its assertion in §7, for example "asserts 1 <= i <= n".
  * Reword the trigger to the real risk: "a caller reconstructs a path for a pair with `minus_infinity(i, j)`, and release builds do not check this precondition".

#### style-3-3. Lines 520 and 535 each pack many separate requirements into one sentence - Severity: Major
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** lines 520, 535
* **Discrepancy Description:**
  * Line 520 chains about 15 harness duties with semicolons: imports, the CLI, the memory check, attempt ids, restart rules, statistics, the cross-check and the `--no-bf-pass` logic.
  * Line 535 mixes VM sizing, the command sequence and the auto-shutdown rules.
  * Restart semantics ("rows of incomplete attempts are discarded") sit in the middle of the sentence and are easy to miss.
* **Rationale & Recommendation:** requirements written this way are hard to cite one at a time or to check off against tests. Use one sub-list item per requirement. Split line 535 into "VM size", "Sequence" and "Auto-shutdown".

### Nits

#### structural-3-2. The `info` row list leaves out `max_abs_weight` - Severity: Nit
* **Reviewer Lens:** `egm-structural`. Also raised as design-3-4 and style-3-4.
* **Affected Scope:** line 486, against lines 214 and 534
* **Discrepancy Description:** §1 says the program prints `info,max_abs_weight`, and the §10 metric table reads it from the `info` row. The §8 row-kind table, which reads as the full stdout contract, does not list it.
* **Rationale & Recommendation:** add `max_abs_weight` to line 486. Also add the Floyd-Warshall `bytes()` figure if the binary prints it (design).

#### design-3-5. Floyd-Warshall rows break the stated `parent[source] == 0` invariant - Severity: Nit
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** lines 383-384, 422, 437-447
* **Discrepancy Description:** when dist(i, i) < 0, `pred(i, i)` is overwritten (line 447 with j = i). So the `row_tree` of a vertex on a negative cycle breaks the invariant promised in the new `parent_tree` comment. `reconstruct_path` still terminates because it stops at `source`, so only the documentation is wrong.
* **Rationale & Recommendation:** choose one fix:
  * Weaken the invariant, for example "parent[source] is not read".
  * Skip j == i in the update.
  * Reset the diagonal of `pred` after the run.

#### design-3-6. "Same answers" across Bellman-Ford variants does not fix how `next` ties are broken - Severity: Nit
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** lines 52, 317-336, 477
* **Discrepancy Description:** the frontier variant scans vertices in insertion order, while stop_when_stable scans them in id order. When two arcs give the same strict improvement, `next[u]` can differ, so the variants can return different paths of equal weight. `variants_agree` compares distances only, but Goal 3 promises "the same answers".
* **Rationale & Recommendation:** either say that only distances and rounds must agree, or sort the frontier by id so `next` is identical too.

#### design-3-7. The watchdog floor equals the step budget - Severity: Nit
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** lines 481, 521
* **Discrepancy Description:**
  * A calibration run is admitted when 8 × the last run's time fits the 600 s budget, and it prints no row until it ends. The watchdog floor is also 600 s, so an extrapolation that comes out slightly low gets the launch killed.
  * The same applies to the first `progress` interval of a slow Floyd-Warshall run: if a pivot takes more than 2.4 s, 250 pivots take longer than 10 min.
* **Rationale & Recommendation:** set the floor above the budget, for example 2 × `--budget-seconds`. Or emit time-based progress rows, for example every 60 s, in both calibrate and `fw`.

#### design-3-8. The cost of `reversed` is understated - Severity: Nit
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** line 228
* **Discrepancy Description:** the design says "O(n + m) time", but `from_edges` sorts each adjacency block (csr.hpp step 4). The real cost is O(n + m log d_max).
* **Rationale & Recommendation:** state the real cost.

#### structural-3-1. The metric table breaks the §10 bullet list - Severity: Nit
* **Reviewer Lens:** `egm-structural`
* **Affected Scope:** lines 519-536
* **Discrepancy Description:** the table at lines 524-534 is not indented under the bullet that introduces it (line 522). No blank line separates it from the next bullet at line 535. In GFM this splits §10 into two lists.
* **Rationale & Recommendation:** add a blank line between lines 534 and 535. Better, move the table after the list, under a lead-in line.

#### structural-3-4. `no_op_pivot` is used but never declared - Severity: Nit
* **Reviewer Lens:** `egm-structural`
* **Affected Scope:** line 427
* **Discrepancy Description:** `no_op_pivot` is the default template argument of `floyd_warshall`, but no listing declares it.
* **Rationale & Recommendation:** add a one-line declaration, for example `struct no_op_pivot { void operator()(vertex_id, vertex_id) const noexcept {} };`.

#### structural-3-5. The heading tags in §§1-7 are inconsistent - Severity: Nit
* **Reviewer Lens:** `egm-structural`
* **Affected Scope:** headings at lines 175, 216, 220, 258, 347, 378, 396
* **Discrepancy Description:**
  * Line 136 says the tags in parentheses map to review findings.
  * Some headings also carry other tags ("io", "owner-approved"), and §4 and §7 have no tag.
  * A reader cannot tell whether a section without a code has no matching finding.
* **Rationale & Recommendation:** use the parentheses for review codes only, or tag every section the same way.

#### structural-3-6. The decision-log links to the review reports have unclear text - Severity: Nit
* **Reviewer Lens:** `egm-structural`
* **Affected Scope:** lines 87-88
* **Discrepancy Description:** the round-1 and round-2 entries both use the link text "report". The file names are not parallel (`goldfish_review_report_round1.md` and `goldfish_review_report.md`), and the same folder also holds the critic reports.
* **Rationale & Recommendation:** use specific link text, for example "round 2 goldfish report". The reviewer does not ask for any file to be renamed.

#### style-3-5. Alternative F gives a tight bound that should be a worst case - Severity: Nit
* **Reviewer Lens:** `egm-style` (CALIBRATE)
* **Affected Scope:** line 586, against line 360
* **Discrepancy Description:** the text says "a Θ(n·m) pass". The check is O(1) when there are no negative weights, and the frontier variant can stop early, so Θ(n·m) is not a tight bound.
* **Rationale & Recommendation:** write "an O(n·m) worst-case pass".

#### style-3-6. Lines 50 and 52 claim more than the evidence covers - Severity: Nit
* **Reviewer Lens:** `egm-style` (CALIBRATE)
* **Affected Scope:** lines 50, 52
* **Discrepancy Description:**
  * Line 50: "(Tests already committed in `879a495`.)" ends a paragraph that also describes `benchmark_part3 --directed`, which does not exist yet.
  * Line 52: "Tests show ... textbook ≥ stop_when_stable ≥ frontier on every graph" claims more than tests can show.
* **Rationale & Recommendation:**
  * Line 50: move the commit reference right after the sentence about the hand-checked direction tests.
  * Line 52: write "on every test graph".

#### style-3-7. The bound on path sums is loose - Severity: Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** line 214
* **Discrepancy Description:** the text says "sums stay below 10^10". The stated inputs give about (n − 1) × 1,000 ≈ 2.5 × 10^7.
* **Rationale & Recommendation:** write the tight figure.

#### style-3-8. Line 455 is one paragraph with five topics - Severity: Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** line 455
* **Discrepancy Description:** the paragraph covers the memory optimization, the `minus_infinity` correctness argument, why skipping row k is safe, NaN behaviour, and the memory and CSR footprint.
* **Rationale & Recommendation:** split it into at least "Correctness with negative cycles" and "Memory".

#### style-3-9. Line 453 does not say where the times are reported or which test checks them - Severity: Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** line 453
* **Discrepancy Description:** "both times are reported" and "checked by a test on random graphs" name no row and no test file.
* **Rationale & Recommendation:** name the row or report table and the test file, for example `tests/test_floyd_warshall.cpp`.

#### style-3-10. The Alternatives entries repeat themselves - Severity: Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** lines 547-707. Examples: G (592/593), K (618/619), Q (656/657).
* **Discrepancy Description:** every entry repeats "Rejected by author", although line 547 already says the author decided every point. Several reasons restate their own Cons.
* **Rationale & Recommendation:** keep the statement at line 547 and drop the per-entry prefix. Limit each reason to the deciding argument.

#### style-3-11. The runtime estimate is repeated in many places - Severity: Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** lines 42, 94, 108, 132, 481, 716
* **Discrepancy Description:** "estimated 5 to 10 h" and "Calibration only estimates" appear several times.
* **Rationale & Recommendation:** give the full statement and its basis once, at line 42, and link to it from the other places.

#### style-3-12. Line 500 has an unclear modifier, and line 509 repeats it - Severity: Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** lines 500, 509
* **Discrepancy Description:** in "`skipped` is only for steps turned off in the config, or the matrix storage, that would fit", it is unclear what "that would fit" applies to.
* **Rationale & Recommendation:** write "`skipped` applies only to a step that would fit but is turned off in the config or uses the matrix storage". Shorten line 509 to "not run by design (see above)".

#### style-3-13. Alternative Y has an unquantified claim - Severity: Nit
* **Reviewer Lens:** `egm-style` (CALIBRATE)
* **Affected Scope:** line 705
* **Discrepancy Description:** "much less memory traffic per pivot" gives no size.
* **Rationale & Recommendation:** give the order of the reduction, for example about B× for a B × B tile, or drop "much".

#### style-3-14. "the owner's `storage/matrix.hpp`" reads oddly - Severity: Nit
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** line 555
* **Discrepancy Description:** the owner wrote the whole project, so the possessive is unclear.
* **Rationale & Recommendation:** say "owned by the project owner and outside this design's scope", or drop "the owner's".

## Re-raised settled findings

None.
