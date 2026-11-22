# Goldfish Review Report (Stage 7 Critic Review, round 1)

**Target:** `docs/designs/part3_design.md` (lines 1-628, everything before "## Detailed Implementation Plan")

**Mode:** REVIEW. **Standards:** egm-design and egm-structural used `resources/document_standards/feature-spec-standard.md`. egm-style used only its persona's rules.

## Reviewer Status

| Reviewer | Status |
| --- | --- |
| `egm-design` | completed |
| `egm-structural` | completed |
| `egm-style` | completed |

## Severity Counts (after deduplication)

| Severity | Count |
| --- | --- |
| Blocker | 2 |
| Major | 15 |
| Nit | 27 |
| **Total** | **44** |

Finding ids use the form `<reviewer>-1-<n>`. When several reviewers reported the same problem, the finding is listed once under the first reviewer's id, and the other ids are given under "Also reported as". Where reviewers disagreed on severity, the higher severity is kept and the disagreement is noted.

---

## Blockers

### design-1-1 - Bellman-Ford misses a negative cycle that is only a negative self-loop - Severity: Blocker
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** lines 276-298 (pseudocode line 287; step 3 at line 298)
* **Problem:** Step 3, the self-loop rule, is listed under "When round n still changes something" (line 294). Negative self-loops are not CSR arcs. So when a self-loop is the only negative cycle reaching the target, or when no other cycle reaches the target, the rounds settle early. Line 287 then stops with "no cycle reaches target" and step 3 never runs. Vertices that reach the loop vertex keep finite distances instead of -inf, and `cycle_reaches_target` stays false. The synthesizer checked this against the pseudocode, and the reading is correct.
* **Rationale:** This brings back the bug that brief finding B1 was raised to fix, and it breaks Goal 4 (line 43). An implementer who follows the pseudocode exactly will ship it.
* **Suggested fix:**
  * Move the self-loop check out of the round-n branch. After the loop ends, for whatever reason, the BFS seeds are the vertices changed in round n plus every loop vertex v with `dist[v] < +inf`.
  * If there are any seeds, run the -inf BFS and set `cycle_reaches_target`.
  * Reword line 287 so it no longer concludes "no cycle".
  * Add a regression test whose only negative cycle is a self-loop on a vertex that reaches the target, and confirm the test fails before the fix.

### structural-1-1 - No Risks and Mitigations section - Severity: Blocker
* **Reviewer Lens:** `egm-structural`, `egm-design`
* **Also reported as:** design-1-8 (rated Major by egm-design)
* **Affected Scope:** the whole range. The new section would go between Alternatives (466-616) and Non-Functional Requirements (620-625).
* **Problem:** feature-spec-standard.md requires the spec to call out its constraints, failure modes and mitigations, and brief M16 (part3.md line 124) asked for a risks section. The risks the design does cover are scattered across lines 34, 47, 65, 104, 425, 562-566 and 622. Several are not named anywhere:
  * An hours-long Floyd-Warshall answer run that dies partway cannot be resumed, so all its VM time is lost.
  * Auto-shutdown or running out of credit can end that run.
  * The c·n³ calibration fit can misjudge the time at n = 25,000 (cache effects, input structure).
  * The FW time estimate is uncertain by up to 5x (1.5-8 h).
  * Nobody knows yet whether directed W_1 has a negative cycle, and the answer changes case studies 1 and 2.
  * `tree_from` checks with an assert, which `-DNDEBUG` removes in the release benchmark (lines 372, 401). CUJ 5 removed exactly this kind of debug-only guard from `path_to_target`.
  * Building `reversed` needs temporary memory on W_5 (about 640 MB, brief M10).
  * The Azure setup clones `origin/feature/part3`, so the run needs a push, and AGENTS.md requires the owner to approve pushes. The design does not list this approval as a step before the Azure run.
* **Rationale:** A reader cannot confirm that every major risk is handled without collecting them from eight places, and some risks are not handled at all.
* **Suggested fix:** Add `## Risks & Mitigations` before the Non-Functional Requirements. Use a table with the columns risk, trigger, impact, likelihood, mitigation, where it is handled, and who decides. Cover every item above.

---

## Major

### design-1-2 - Floyd-Warshall gives no answer and no defined cross-check when a negative cycle exists anywhere - Severity: Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** lines 79, 372, 398-401, 441, 445
* **Problem:** Line 401 says every entry except the witness is "unspecified", and `tree_from` asserts. If directed W_1 has any negative cycle, even one that cannot affect {10,...,50}:
  * case study 2 gets no distances;
  * the FW `path` rows have no defined status;
  * the exact `mismatch` cross-check against Bellman-Ford means nothing, because Bellman-Ford still gives a finite or -inf answer for each vertex.
* **Rationale:** Without an early stop, dist(i,j) is always the weight of some walk from i to j. dist(i,j) is -inf exactly when some k has dist(i,k) < inf, dist(k,k) < 0 and dist(k,j) < inf. That test costs O(n) per pair, so about 20n for the requested pairs, which is negligible next to n³.
* **Suggested fix:**
  * Define a per-pair classification after FW (`ok` with a distance, or `negative_cycle`), either as a member such as `all_pairs::is_minus_infinity(i, j)` or as a step in the program.
  * Say which `path` rows `fw` mode emits when `negative_cycle()` is true.
  * Make `mismatch` compare the classifications as well as the values.
  * Extend CUJ 2 to cover W_1 with a cycle.
  * Fix the wording at line 445: Bellman-Ford toward t matches the FW column dist(·, t), not a row.

### design-1-3 - The negative-cycle witnesses are never checked, and the cycle weight is missing - Severity: Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** lines 296, 319, 375, 398, 433
* **Problem:** The witnesses come from walking pointers that in-place updates have overwritten many times. Getting a cycle out of FW pred row i is a known subtle case. The `cycle` row has no weight column, although brief M5 asked to report the cycle's sum.
* **Rationale:** The case-study table presents the witness as proof. A wrong witness (a cycle that is not negative, or steps that are not arcs) would go unnoticed.
* **Suggested fix:**
  * Check every consecutive pair of the witness against the graph. CSR neighbour lists are sorted, so a binary search works; count negative self-loops as arcs.
  * Sum the weights, require the sum to be < 0, and print it: `cycle,<found>,<length>,<weight>,<vertices>`.
  * Report `error` if the check fails.
  * Add randomized tests with planted cycles for all three witness producers.

### design-1-4 - The Floyd-Warshall mean comes from a random-graph fit even though a run on the real graph is measured - Severity: Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** lines 47, 79, 103, 424; Alternatives 586-616
* **Problem:**
  * At n = 25,000 the reported mean comes from a c·n³ fit on random digraphs with n of at most about 8-16k, while the one real run is only recorded "next to" it.
  * The row skip (line 394) makes the cost depend on reachability. Random digraphs with average out-degree 22-33 are almost fully strongly connected, so the fit can miss the structure of the real W_1 and W_2.
  * The brief (B6) proposed timing k-blocks on the real graph. The design does not list that as an alternative, although the Alternatives Rule requires it.
* **Rationale:** The report would show two figures for the same quantity that disagree. The CUJ 6 VM-time estimate comes from the same fit.
* **Suggested fix:**
  * Report the measured run as the per-run time (status `fewer_runs` or `single_run`, samples = 1).
  * Keep the fit only for the pre-run VM estimate and for W_3-W_5, or calibrate by timing k-blocks on the real graph.
  * Add the k-block alternative with its accept or reject reason.

### design-1-5 - A step that measures exactly one run within the budget has no status - Severity: Major
* **Reviewer Lens:** `egm-design`, `egm-style`
* **Also reported as:** part of style-1-5
* **Affected Scope:** lines 47, 420-424, 442, 446
* **Problem:** A first run between 300 s and 600 s with a 600 s budget leaves 1 sample. Neither `ok` (10 runs), `fewer_runs` (2-9) nor `extrapolated` (first run over the budget) covers it. Likely cases are the allpairs passes, the `off` variant, and Bellman-Ford on W_5 (20-100 s per pass).
* **Rationale:** This is the "single sample shown as a mean" defect that Alternative S2 (lines 594-598) was rejected for.
* **Suggested fix:** Extend `fewer_runs` to 1-9 runs, or add `single_run`, and state that samples = 1 has no spread.

### design-1-6 - Azure auto-shutdown cannot cover the planned run length, and a killed Floyd-Warshall run loses hours - Severity: Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** lines 47, 65, 104, 457, 623
* **Problem:**
  * `az vm auto-shutdown --time HHMM` is a daily schedule, so it can be at most 24 h ahead.
  * The upper estimate is about 20-22 h: 2 FW runs of up to 8 h, the repeated steps, and calibration. The roughly 23 h shutdown is set at `create`, before setup and upload.
  * Deallocation kills the tmux job. An FW run prints nothing until it finishes, so "a run that dies keeps what was already measured" does not help it.
* **Rationale:** The safety net can fire in the middle of the run that matters most.
* **Suggested fix:**
  * Reset the shutdown to now + estimate + margin before `start`, and refuse to start if the estimate is over about 22 h. Or use one session per FW graph.
  * Have the FW run print progress lines (k done, elapsed) to stderr so `status` can show them.
  * Record the risk in the new Risks section (structural-1-1).

### design-1-7 - Resume and timeout behaviour is too loose to prevent duplicate or partial rows - Severity: Major
* **Reviewer Lens:** `egm-design`
* **Affected Scope:** lines 425, 455-456
* **Problem:**
  * "Complete" (graph, mode) pairs are skipped on restart, but "complete" is never defined. A step that dies after 4 flushed `time` rows, followed by a rerun, mixes 14 rows from two attempts into one mean.
  * "One timeout per launch" gives no value. A fixed value either kills the FW answer run or never catches a hang.
* **Rationale:** Brief M14 asked for resumable runs. The schema exists, but the protocol that makes resuming safe does not.
* **Suggested fix:**
  * Have the binary print a terminator row per mode (for example `done,<mode>`), and count a pair as complete only when that row is present.
  * On restart, drop the rows of any incomplete pair, or tag rows with an attempt id and aggregate only the last complete attempt.
  * Derive the timeout from the calibration: estimate × safety factor for `fw`, and budget × runs + margin for the other modes.

### structural-1-2 - The status of the skipped Floyd-Warshall runs on W_3-W_5 contradicts itself - Severity: Major
* **Reviewer Lens:** `egm-structural`, `egm-style`
* **Also reported as:** style-1-4
* **Affected Scope:** lines 45, 54, 85, 443-444
* **Problem:** Three places give three different statuses for the same case:
  * line 54 says "reported as skipped";
  * CUJ 3 (line 85) says `no_memory` "(or `skipped` for time)";
  * the status table (line 444) says `no_memory`.

  In addition, `skipped` (line 443) is defined as "matrix storage, or a step turned off in the config", with no time-based case.
* **Rationale:** The status table is the contract between the binary, the harness and the report, and three different wordings lead to disagreeing code.
* **Suggested fix:**
  * Use `no_memory` at line 54.
  * Either drop "(or `skipped` for time)" at line 85, or add a time reason to the `skipped` definition.
  * Make Goal 6's "skipped or extrapolated" list every non-`ok` status, or point to section 9.

### structural-1-3 - `extrapolated` overlaps `fewer_runs` and does not match Goal 8 - Severity: Major
* **Reviewer Lens:** `egm-structural`, `egm-style`
* **Also reported as:** style-1-5
* **Affected Scope:** lines 47, 73, 91, 442, 446
* **Problem:**
  * Line 442 defines `extrapolated` as "a statistic computed from fewer runs than asked", which also describes `fewer_runs` (2-9 runs).
  * Goal 8 uses `extrapolated` for model-based estimates: a first run over the budget, or n × mean.
  * CUJ 1 (line 73) promises "mean and median over 10 runs" even though Goal 8 allows fewer.
* **Rationale:** A reader cannot tell which status a 3-run mean gets.
* **Suggested fix:**
  * Define `extrapolated` as "not measured; computed from a model (calibration fit or n × per-pass mean), with the formula given".
  * Keep `fewer_runs` for real statistics over measured runs.
  * Change line 73 to "over up to 10 runs, with the sample count".

### structural-1-4 - The "row kinds" column of the status table does not match the row schema - Severity: Major
* **Reviewer Lens:** `egm-structural`, `egm-style`
* **Also reported as:** style-1-18 (rated Nit by egm-style)
* **Affected Scope:** lines 420, 427-433, 439-448
* **Problem:**
  * `ok` lists the row kind `time`, but `time` rows (line 430) have no status column.
  * `mismatch` lists only `path`, but line 420 also uses it for optimization variants that disagree, which produce `time`/`info` rows.
  * "results (harness)" is a harness CSV, not a row kind of the binary, and `ok` does not list it although Goal 8 assigns `ok` to results.
  * `cycle` rows (line 433) have no status column.
  * The `time` row's algorithm list omits `bellman_ford_pass_<variant>`.
* **Rationale:** The two tables are the interface spec and must agree exactly.
* **Suggested fix:**
  * Rename the column to "emitted in" and give explicit values (`stdout: path`, `stdout: estimate`, `results_part3.csv`).
  * Remove `time` from `ok`, or give `time` rows a status.
  * Say where a variant `mismatch` is written, and decide whether `cycle` rows carry a status.
  * Align both tables with line 420.

### structural-1-5 - Rejection attributions point to sources that cannot be opened - Severity: Major
* **Reviewer Lens:** `egm-structural`
* **Affected Scope:** line 468; every "Reason for Rejection: Rejected by author" from 476 to 616
* **Problem:** The Alternatives Rule requires a link when a rejection is credited to a maintainer. Line 468 cites "the drafting conversation of 2026-11-21", which cannot be linked, and names `docs/designs/part3.md` as a code span rather than a link. The technical reasons themselves are concrete and meet the rule.
* **Reviewer disagreement:** egm-design judged that attribution passes because every rejection gives concrete trade-offs and points to the brief. The finding is kept at Major because the link requirement is a literal rule of the standard. The owner may downgrade it.
* **Suggested fix:**
  * Make part3.md a relative Markdown link, with anchors to "Alternatives Weighed" and "Decisions after review round 1".
  * Drop the conversation reference, or first record those decisions in part3.md and link them.
  * Optionally drop the repeated "Rejected by author" prefix.

### style-1-1 - "One named constant (100)" contradicts section 1 - Severity: Major
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** lines 64, 177, 418
* **Problem:** Line 64 names the constant as 100. Lines 177 and 418 say the named constant is the number of decimals (2), and that the scale (100) comes from `fixed_point_weight::scale()`.
* **Rationale:** An implementer who follows line 64 will hard-code 100.
* **Suggested fix:** At line 64, say "one named constant, the number of decimals (2); the scale (100) is derived from it", and shorten the entry to the decision plus a pointer to section 1 (see structural-1-6).

### style-1-2 - Wrong claim that a bare `std::span` satisfies `parent_tree` - Severity: Major
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** line 345 (concept at lines 337-342)
* **Problem:** "a `std::span<const vertex_id>` now does too" is false. The concept requires `source` and `parent` members. The type that qualifies is `all_pairs<W>::row_tree`.
* **Rationale:** The claim contradicts the definition next to it and could lead to a wrong test.
* **Suggested fix:** "`all_pairs<W>::row_tree`, whose `parent` is a `std::span<const vertex_id>`, now qualifies too."

### style-1-3 - "Part 1 and Part 2 code stay as they are" is overstated - Severity: Major
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** line 20
* **Problem:** Sections 1, 2 and 6 change committed library code:
  * a new `read_edge_list` overload with a shared body;
  * a changed `edge_list` field;
  * a loosened `parent_tree`;
  * a step bound in `reconstruct_path`.

  Lines 56 and 625 correctly limit "unchanged" to programs and results.
* **Rationale:** The claim promises more than the plan does.
* **Suggested fix:** "Part 1 and Part 2 programs and results stay as they are. Library changes are limited to `io/edge_list.hpp` (sections 1-2) and `algorithms/path.hpp` (section 6)."

### style-1-6 - "Works with the existing `reconstruct_path`" is miscalibrated - Severity: Major
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** line 44
* **Problem:** Row views only work after the section 6 change to `parent_tree`. The current concept (path.hpp line 16) requires `const std::vector<vertex_id>&`.
* **Rationale:** "Existing" suggests that no library change is needed, which is false.
* **Suggested fix:** "Works with `reconstruct_path` (after the `parent_tree` change in section 6) without copying."

### style-1-7 - Goal 8 mixes the measurable goal with history and side topics - Severity: Major
* **Reviewer Lens:** `egm-style`
* **Affected Scope:** line 47
* **Problem:** One paragraph of about 200 words mixes:
  * the budget rule;
  * the status semantics;
  * the history of the rejected 20 s limit, which Alternative S2 already records;
  * the single answer-run rule;
  * Azure cost policy, which the NFRs already cover;
  * crash resilience.

  "Azure use stays modest" cannot be measured.
* **Rationale:** A goal should state measurable outcomes, and history belongs in Alternatives Considered.
* **Suggested fix:**
  * Drop the 20 s history.
  * Replace "modest" with the measurable rule: one VM of the smallest size that fits, deleted after the results are fetched.
  * Split the rest into sub-bullets: budget rule, statuses, answer runs, resilience.

---

## Nits

### design-1-9 - The template argument of `read_edge_list` cannot be deduced - Severity: Nit
* **Lines:** 150-153, 418. W appears only in the return type, so `read_edge_list(path, io::fixed_point_weight<double>{weight_decimals})` does not compile. **Fix:** write `read_edge_list<gl::weighted<double>>(...)` as at line 116, or default W from the parser's result type.

### design-1-10 - The exactness bound covers single weights, not path sums - Severity: Nit
* **Lines:** 64, 165-167. The parser rejects a single weight above 2^53, but exact comparison also needs (n - 1) × max|w| × 100 ≤ 2^53. **Fix:** state that condition, or check it after reading the file.

### design-1-11 - `path_to_target` rule order is unclear when the target itself is -inf - Severity: Nit
* **Lines:** 265-267. "{target} when v == target" and "empty when dist is ±inf" conflict when a negative cycle runs through the target. **Fix:** say which rule applies first; returning empty is consistent with Goal 4.

### design-1-12 - The `frontier` and `both` variants do the same relaxations - Severity: Nit
* **Lines:** 278, 287, 413. With frontier_only on, an empty frontier relaxes nothing, so the two variants are not independent measurements. **Fix:** say so, and isolate optimization 1 by comparing `stop` against `off`.

### design-1-13 - Unclear which time counts as "the Bellman-Ford time" - Severity: Nit
* **Lines:** 420, 430. **Fix:** state whether the reversal is included in the reported time (`reverse` + `bellman_ford_pass`) or reported separately.

### design-1-14 - The all-pairs estimate formula is incomplete - Severity: Nit
* **Lines:** 91, 423. "n × mean" leaves out the one reversal (and possibly the whole-graph check). The design also does not say what the FW side of the comparison is for W_3-W_5. **Fix:** give the full formula, and report the spread next to n × mean.

### design-1-15 - The calibration can overshoot the budget - Severity: Nit
* **Lines:** 103, 424. When sizes double, the next run can cost 8x the last one. **Fix:** apply Goal 8's rule that a run starts only if its predicted time fits the budget.

### design-1-16 - Alternative F miscounts the passes - Severity: Nit
* **Lines:** 507. "3 × 10 times" should be 1 × 10, because one pass answers all three sources (line 420).

### design-1-17 - It is not said where the Bellman-Ford/FW cross-check runs - Severity: Nit
* **Lines:** 445, 610. No mode in section 8 runs the 5 passes toward {10,...,50} or emits the comparison. **Fix:** say whether `fw` mode runs them or the harness joins the `bf` rows.

### structural-1-6 - Open Questions has almost nothing open - Severity: Nit
* **Lines:** 59-65. Three of the four bullets are already resolved, line 64 repeats section 1, and a blank line at 63 splits the list in two. **Fix:** rename the section "Decisions and Open Questions" or add a decision-log H3, shorten line 64, and keep line 65 as the only open item.

### structural-1-7 - Headings carry review codes the design never defines - Severity: Nit
* **Also reported as:** style-1-19
* **Lines:** 141, 179, 183, 302, 329. The codes B1, B4, M10, B3 and M6 come from part3.md. Sections 4, 7 and 8, which also answer findings, are not tagged. **Fix:** drop the codes, or add one line under Technical Plan explaining them and tag every section the same way.

### structural-1-8 - Bullet marker spacing varies - Severity: Nit
* **Lines:** 3-5, 61-65, 71-104 and 622-625 use `*   `; 28-32, 51-57, 137-139, 417-425 and 454-464 use `* `. **Fix:** pick one.

### structural-1-9 - CUJ fields and Alternatives labels are uneven - Severity: Nit
* **Lines:** 104, 594. CUJ 6 has an extra "Budget" field. The Alternatives run A...S, S2, T...V. **Fix:** move the Budget content to the NFRs or the Risks section, and relabel S2.

### structural-1-10 - Horizontal rules and Technical Plan numbering are inconsistent - Severity: Nit
* **Lines:** 7, 618, 627; 108, 135. Rules appear only after the metadata and around the NFRs. Data flow and Glossary are unnumbered H3s in front of numbered ones. **Fix:** use rules consistently, and number or group the two unnumbered H3s.

### structural-1-11 - Two small gaps in the API sketches - Severity: Nit
* **Lines:** 263, 429. The `bellman_ford` convenience signature ends in `/* same tail */` and has no `requires` clause. The `info` metric `negative_self_loops` keeps the old field's name. **Fix:** write out the full signature, and rename the metric or define it as the size of `negative_self_loop_edges`.

### style-1-8 - Hyperbole about grafo_W_1 - Severity: Nit
* **Lines:** 29. "No algorithm that answers anything" is too strong, because BFS and DFS still run on it. **Fix:** "no shortest-path algorithm accepts it".

### style-1-9 - "If reused naively" is vague - Severity: Nit
* **Lines:** 30. **Fix:** "if reused unchanged".

### style-1-10 - Large-graph time estimates have no basis - Severity: Nit
* **Lines:** 34. "Weeks" and "days to months" are not backed by anything. **Fix:** give the c·n³ basis, or label them "estimated, not measured".

### style-1-11 - Filler lead-in - Severity: Nit
* **Lines:** 36. **Fix:** "Proposed solution:".

### style-1-12 - Redundant history sentence - Severity: Nit
* **Lines:** 97. Line 61 already records it. **Fix:** drop "This replaces the earlier debug-only assertion."

### style-1-13 - "(estimated hours)" is vague - Severity: Nit
* **Lines:** 79. **Fix:** "(estimated 1.5 to 8 h)".

### style-1-14 - "A copy of `azure_part2.sh`" understates the changes - Severity: Nit
* **Lines:** 133 vs 457. **Fix:** "adapted from `azure_part2.sh` (section 10)".

### style-1-15 - "Existing tests prove it" - Severity: Nit
* **Lines:** 156. **Fix:** "the existing io tests, which assert the error messages, must still pass" (only if they do assert the messages).

### style-1-16 - Two different flush rules - Severity: Nit
* **Lines:** 129 vs 425. "Per repetition" and "after each row" disagree. **Fix:** use one rule (see also design-1-7).

### style-1-17 - Past tense in a proposed design - Severity: Nit
* **Lines:** 345. **Fix:** "Goes in its own commit".

### style-1-20 - Unquantified claims in the Alternatives - Severity: Nit
* **Lines:** 532, 550. "Much faster" and "often fewer relaxations" are not quantified. **Fix:** quantify them or drop them.

### style-1-21 - Mixed memory units - Severity: Nit
* **Lines:** 615 uses GiB, and everywhere else uses GB. **Fix:** pick one.

---

## Merge Notes

* style-1-18 (the `time` and `mismatch` row kinds) was merged into structural-1-4. style-1-19 (review codes in headings) was merged into structural-1-7.
* Checks that passed, as reported:
  * the memory figures (7.5 GB, 120 GB, 12 TB, 1.2 PB) and the CSR sizes;
  * the Bellman-Ford round-n detection and the -inf marking argument (sound once design-1-1 is fixed);
  * the `reversed_graph`, `reverse_input`, loosened `parent_tree` and lvalue-only `tree_from` designs;
  * the security posture;
  * fenced blocks, table shapes and heading hierarchy.
* Out of scope: brief M16 also asked for a traceability table, an acceptance table and a test strategy. These may be in "Detailed Implementation Plan" (line 629 on), which this round did not review.
