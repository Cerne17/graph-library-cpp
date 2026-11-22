# Part 3: directed graphs, Bellman-Ford, Floyd-Warshall

* **Author:** Cerne17
* **Date:** 2026-11-21
* **Status:** concepts brief, superseded by the design [part3_design.md](part3_design.md) (2026-11-21). Where they differ, the design wins.

## Overview

Part 3 of the COS242 assignment (`trabalho-parte3.pdf`) asks the graph library to:

1. represent directed weighted graphs, with a user parameter saying whether the file is directed (arc = first id → second id, same file format as Part 2);
2. run the existing BFS, DFS and Dijkstra on directed graphs;
3. run Bellman-Ford "de todos os vértices para um vértice específico", returning distances, the shortest-path tree, and whether the graph has a negative cycle, with the two optimizations from class;
4. run Floyd-Warshall for all pairs, returning distances, the shortest-path tree from each vertex, and whether the graph has a negative cycle, with the memory optimization from class.

Case studies, for each graph: Bellman-Ford distances from 10, 20, 30 to 100; Floyd-Warshall distances among {10, 20, 30, 40, 50}; mean time of each algorithm over 10 runs in seconds, without disk I/O; all-pairs by repeated Bellman-Ford against Floyd-Warshall, and which is faster and why.

The work extends the existing header-only library in `include/graph/` (C++23 templates, compile-time policies, concepts `graph_like` and `weighted_graph`). It does not redesign it. Part 1 code is untouched. Part 2 programs are untouched; Part 3 gets its own programs next to them.

## Current state

* `main` (local) holds origin/main plus the 14 `feature/harness-part2` commits, replayed. Not pushed.
* `feature/part3`, from `main`, has two commits:
  * `d9c465a feat(io): count negative self-loops in read_edge_list` — new field `io::edge_list::negative_self_loops`.
  * `9dadcea test: directed graphs where direction changes bfs, dfs, dijkstra and load_graph results`.
  `make test` passes (186 cases).
* A Bellman-Ford prototype written before this review (`reversed.hpp`, `algorithms/bellman_ford.hpp`, `tests/test_reversed.cpp`) is parked in `git stash` as "part3: bellman-ford prototype (pre-review)". It is input for later, not a decision.

## Facts the design depends on

* BFS, DFS and the three Dijkstra versions only call `for_each_neighbor` / `for_each_edge`, which yield out-arcs on a directed storage. They already work on digraphs. CSR and matrix both branch on `D::is_directed` when built.
* `read_edge_list` drops self-loops. A negative self-loop is a negative cycle of one arc, so it is now counted in `negative_self_loops` and the programs must report it.
* An undirected graph with a negative edge always has a negative cycle (u–v–u).
* Case-study graphs, in `data/part3/` (not committed):

  | file | n | m | negative edges | note |
  |---|---|---|---|---|
  | grafo_W_1 | 25,000 | 549,953 | 4,998 | only graph with negative weights |
  | grafo_W_2 | 25,000 | 824,964 | 0 | |
  | grafo_W_3 | 100,000 | 6,099,941 | 0 | same file as Part 2 |
  | grafo_W_4 | 1,000,000 | | 0 (Part 2) | same file as Part 2 |
  | grafo_W_5 | 10,000,000 | | 0 (Part 2) | same file as Part 2 |

  None has self-loops among those checked (W_1 to W_3).
* Floyd-Warshall memory with `double` distances and `vertex_id` predecessors: `(n + 1)² × 12` bytes. n = 25,000: about 7.5 GB (fits the owner's 16 GiB Mac). n = 100,000: 120 GB. Larger: terabytes or more.
* Floyd-Warshall time: n³ relaxations. Estimate at 1–2 ns each, not measured: n = 25,000 → 1.6e13 → 4 to 8 h per run, 2 to 4 days for 10 runs. n = 100,000 → weeks.
* Bellman-Ford for all pairs: n single-target runs. On grafo_W_3, if one run costs 0.5 to 1 s (Dijkstra with a heap measured 0.118 s there in Part 2), 10 repetitions take 6 to 12 days. Not measured.

## Agreed decisions

### a. Direction flag

* Programs take `--directed yes|no`, default `no`. They map flag × representation to one of four template instantiations (`csr_graph` / `matrix_graph` × `undirected` / `directed`, weight `double`). The library stays compile-time.
* `read_edge_list` and `load_graph` do not change behaviour. `negative_self_loops` was added (done).

### b. Bellman-Ford toward one target

* New header `include/graph/reversed.hpp`: `gl::reversed(g)` returns a `csr_graph<directed, W>` with every arc turned around. No change to `concepts.hpp` and no `for_each_in_edge`.
* Bellman-Ford toward t = single-source Bellman-Ford from t on the reversed graph. For an undirected graph the reversal is skipped.
* A second entry point takes an already reversed graph, so many targets can share one reversal.
* Timing counts the reversal inside the Bellman-Ford time, and also reports it on its own row.

### c. The two optimizations

1. Stop as soon as a full round changes no distance.
2. In each round, relax only the edges around vertices whose distance changed in the previous round.

Rounds stay synchronous (a frontier per round), not a FIFO queue, so a change in round n still means a negative cycle. Options switch each optimization off, and a stats out-parameter counts rounds and relaxations, so tests can show each optimization prunes work and the report can compare them.

### d. Negative cycle meaning: any negative cycle in the graph

The result says whether the graph has a negative cycle anywhere (the literal reading of "se o grafo possui um ciclo negativo"). It also says whether one of them can reach the target, because only that kind makes the requested distances undefined. Finding cycles that cannot reach the target needs a second pass from a virtual source with an arc of weight 0 to every vertex, run when the first pass found none. Floyd-Warshall gives the same meaning (some `dist[i][i] < 0`).

### e. Result types

* Bellman-Ford returns a new `target_paths<W>`: `target`, `dist[v]` (v → target), `next[v]` (vertex after v on that path), the two cycle flags, and the vertices of one negative cycle as a witness. It deliberately does not satisfy `parent_tree`, because `reconstruct_path` would return those paths reversed. A new `path_to_target(tree, v)` returns v first.
* Negative cycles are reported in the result, not thrown. Dijkstra's exception guards an input precondition; a negative cycle is a result.
* `parent_tree` in `algorithms/path.hpp` is loosened from "convertible to `const std::vector<vertex_id>&`" to "random-access range of `vertex_id`", so a Floyd-Warshall row view works with `reconstruct_path` unchanged. `search_tree` and `shortest_paths` still satisfy it.

### f. Floyd-Warshall

* One (n + 1) × (n + 1) distance matrix updated in place for each k (the memory optimization), plus a predecessor matrix: `pred[i][j]` = vertex before j on the shortest path from i, so row i is the tree rooted at i.
* `tree_from(i)` returns a no-copy view (`source`, `parent` span, `dist` span) that satisfies `parent_tree`.
* Negative cycle when some `dist[i][i] < 0`; the run stops early when one appears.
* Rows are n + 1 wide so spans index by 1-based vertex id.

### g. Names, files, output

* Library: `algorithms/bellman_ford.hpp`, `algorithms/floyd_warshall.hpp`, `reversed.hpp`. Tests: `test_bellman_ford.cpp`, `test_bellman_ford_differential.cpp`, `test_floyd_warshall.cpp`, `test_reversed.cpp`.
* Programs with suffix `_part3`: `app/benchmark_part3.cpp`, `analysis/harness_part3.py`, `config_part3.csv`, `data/results_part3.csv`, `data/paths_part3.csv`, `make benchmark-part3`.
* Output rows shaped like Part 2: `info`, `time` (microseconds, converted to seconds by the report script), `dist`, `path`. Statuses: `ok`, `unreachable`, `negative_cycle`, `skipped`, `extrapolated`, `mismatch`, `no_memory`, `out_of_range`.
* Where all-pairs Bellman-Ford is out of reach, time k = 100 sampled targets (fixed seed) and report `n × mean` with status `extrapolated`.

## Analysis (Goldfish review, 2026-11-21)

Seven reviewers (design, codebase details, zoom-out, comprehension, implementation readiness, structure, inventive catalyst) read this brief and the files it builds on. None asked to reverse decisions a–g. After merging duplicates there are 6 blockers, 16 major findings and about 15 nits. Brackets give how many reviewers raised each point. Limits of the review: the assignment PDF is not in the repo, so requirements were traced only against the Overview, and one reviewer had no shell, so the test count and the file identity claims were not re-checked.

### Blockers

* **B1. Negative self-loops never reach the algorithms [6/7].** `edge_list` keeps only a count, `load_graph` drops it, and both storages reject self-loops. So Bellman-Ford's "any cycle" flag is false for a file whose only negative cycle is a self-loop, "reaches target" cannot be computed, and Floyd-Warshall's `dist[i][i] < 0` rule never fires (the diagonal starts at 0). Options: (a) keep the vertex ids in `edge_list` and pass them to the algorithms as an optional span: Floyd-Warshall starts `dist[v][v] = min(0, w)`, Bellman-Ford marks the cycle as reaching t when `dist[v] < inf`. This is a further io change and needs the owner's OK. (b) A program-level rule: any negative self-loop makes the whole graph `negative_cycle`. Also: W_4 and W_5 were never checked for self-loops.
* **B2. Path walks can loop forever after a negative cycle [7/7].** `reconstruct_path` walks parents with no bound (`path.hpp:37-39`); Bellman-Ford's `next` and Floyd-Warshall's `pred` can contain a cycle. The benchmark would hang, and the debug modes do not catch an endless loop. Fix: write down that `dist`, `next`, `pred` are unspecified when a cycle makes them undefined; `tree_from` and `path_to_target` assert no such cycle; `path_to_target` stops after n steps; the program prints `negative_cycle` without calling a path function. Spell out the predecessor rules: `pred[i][i] = 0`, `pred[i][j] = i` for an arc, 0 when unreachable, update `pred[i][j] = pred[k][j]`.
* **B3. The whole-graph cycle pass runs once per target [7/7].** "Any negative cycle" is a property of the graph, but the brief reruns a Θ(nm) pass after every target pass that finds none: 3 targets × 10 runs in case 1, n times in all-pairs. That also makes the Bellman-Ford vs Floyd-Warshall comparison unfair, since Floyd-Warshall detects once. Consensus fix: a separate graph-level function, run at most once per graph with its own time row; O(1) answer "no" when the graph has no negative weight and no negative self-loop (W_2 to W_5); O(1) answer "yes" for an undirected graph with a negative edge. No extra vertex is built (`from_edges` asserts ids ≥ 1): start every distance at 0 and put every vertex in the first frontier. In a full all-pairs run the pass is redundant, since the union of the per-target "reaches t" flags is "any cycle".
* **B4. The "already reversed" entry point accepts the wrong graph silently [5/7].** Passing the original directed CSR compiles and returns distances from t labelled as distances to t, the same kind of silent error the brief rejects elsewhere. Options: (a) `reversed(g)` returns a distinct `reversed_graph<G>` type and the shared entry point accepts only that (or an undirected graph), with a negative test; (b) a public single-source `bellman_ford(g, s)` returning `shortest_paths<W>` (which already satisfies `parent_tree`) plus a thin `bellman_ford_to(g, t)` that reverses and renames `parent` to `next`. Also say what `reversed` does on an undirected graph.
* **B5. Signatures and output schemas are not written down [3/7].** Missing: the exact fields, sizes and sentinel values of `target_paths`, the witness format and orientation, `path_to_target` edge cases, the Floyd-Warshall result type and view, the options/stats structs (does "relaxation" mean arc scanned or distance improved?), the new `dist` row columns, new `info` metrics, time-row names, the CSV headers, the `config_part3.csv` columns, and the full command line. The Part 2 harness raises on any unknown row kind, so the schema is load-bearing.
* **B6. The measurement protocol is not defined [6/7].** Is one Bellman-Ford "run" one pass toward 100 (which answers 10, 20 and 30 at once) or three? Is Floyd-Warshall's matrix allocation timed? Is the reversal shared across the 10 runs? Proposed all-pairs formula: one reversal + n × mean pass time + one cycle pass, recorded on each extrapolated row. The k = 100 sample reports no spread, and targets reached by a cycle cost Θ(nm), which skews the mean. Calibrate before any long run: time Floyd-Warshall at n = 1k, 2k, 4k, 8k and fit c·n³ (one reviewer expects the loop to be limited by memory bandwidth, about 1.5–3 h per 25k run, so the 4–8 h guess may be off either way), or time the first few hundred values of k on the real graph and scale; time one Bellman-Ford run per graph before choosing k.

### Major findings

* **M1. Default `--directed no` is unsafe for W_1 [6/7].** Every W_1 answer would silently become `negative_cycle`. Make direction a required config column, print `info,directed,...` on every run, consider making the flag mandatory.
* **M2. Bellman-Ford on a matrix actually runs on CSR [6/7].** `reversed` always returns CSR, and building it from a 25k matrix is an O(n²) scan counted as reversal time, while undirected matrix runs skip the reversal. Options: run Bellman-Ford and Floyd-Warshall on CSR only and mark matrix `skipped`; make `reversed` return the input's storage kind; or label the rows honestly.
* **M3. Floyd-Warshall memory and allocation [6/7].** A matrix input (5 GB) plus 7.5 GB of Floyd-Warshall cells is about 12.5 GB on a 16 GiB Mac: swapping would spoil the timing. Build from CSR or free the graph after initialisation. The harness guard must use (n+1)²×12 plus the graph, checked before launch; macOS may swap instead of throwing `bad_alloc`. Measure peak memory (`ru_maxrss`), not only RSS at READY. Index cells with `std::size_t` (32-bit overflow above about 65k).
* **M4. Floyd-Warshall early stop [5/7].** Stopping at the first `dist[k][k] < 0` makes its time incomparable with all-pairs Bellman-Ford, throws away distances that are still defined, and can make the two algorithms disagree. Reviewers split: always run every k and check the diagonal at the end, or check once after each complete k. Either way, report the k where it stopped.
* **M5. Rounding can create a "negative" cycle [5/7].** A cycle that weighs exactly 0 can sum to about −1e−16 in floating point, and the two algorithms add in different orders. W_1 has 4,998 negative weights; W_3 has 3,074 zero weights. Pick one tolerance rule shared by both algorithms and the cross-checks, report the witness cycle's sum, and test a zero cycle that rounds negative.
* **M6. Loosening `parent_tree` needs a precise rule [6/7].** `reconstruct_path` calls `.size()`, so use random-access + sized range with value type `vertex_id`; one negative test per clause (`std::list<vertex_id>`, `std::vector<int>`); `static_assert(!parent_tree<target_paths<…>>)`; own commit before Floyd-Warshall.
* **M7. Row views can dangle [5/7].** `floyd_warshall(g).tree_from(1)` points into a temporary, and AddressSanitizer is off on macOS. Make `tree_from` lvalue-only and test it. Counterpoint: copying the 5 rows the case study needs costs nothing measurable, so the view may not be worth it.
* **M8. Test plan lacks independent references for negative weights [6/7].** Potentials (`w' = w + p(u) − p(v)`, w ≥ 0) for graphs without negative cycles, planted cycles of known weight, Bellman-Ford(t) against Floyd-Warshall column t, brute force for n ≤ 6, a cycle through all n vertices, a negative 2-cycle, all four option combinations agreeing, and pruning shown by the stats.
* **M9. One round is not specified [5/7].** Pseudocode must settle: which arcs (out-arcs of changed vertices in the reversed graph), whether a round sees values updated in the same round, the exact round that signals a cycle in each pass, and how empty rounds count. With both optimizations off, directed W_3 needs about 1e12 relaxations: limit the on/off comparison to W_1, W_2 or synthetic graphs. One reviewer proposes dropping the switches and comparing against a textbook Bellman-Ford written in the tests.
* **M10. How `reversed` is built and where it lives [4/7].** The CSR constructor is private, so `reversed` goes through `from_edges`: a temporary array of m edges (about 640 MB on W_5) and a sort per vertex, all inside the timed region. It is an operation, so by CONVENTIONS.md it belongs in `algorithms/` (`gl::algorithms`), or the conventions table changes.
* **M11. One flag per target decides all three sources [1/7].** "Some cycle reaches 100" does not make dist(10 → 100) undefined. A BFS on the reversed graph from the vertices still changing in round n marks exactly the vertices whose distance is −∞, in O(n + m). This reverses a stated non-goal.
* **M12. Floyd-Warshall from CSR [3/7].** CSR keeps parallel arcs (W_3 has 73): initialisation must take the minimum. Skipping row i when `dist[i][k] = inf` is still Floyd-Warshall and saves a lot on sparse graphs; decide whether it is allowed.
* **M13. Status list incomplete [5/7].** Needs a table of status, row kinds, meaning and the `mismatch` tolerance; `not_implemented` (Dijkstra on directed W_1) and `error` were dropped without saying why; "a cycle exists but does not reach the target" has no representation.
* **M14. Multi-day runs cannot resume [3/7].** The Part 2 harness reads output only when the process ends, so a run that dies at repetition 9 loses everything. Print and flush one row per repetition, resume from recorded samples, timeouts, progress on stderr. `azure_part2.sh` is hard-wired to Part 2 names and branch.
* **M15. Git and docs hygiene [4/7].** `data/part3/` is not ignored by `.gitignore`. Branching `feature/part3` from `main` contradicts AGENTS.md (branch from `feature/generalization`); the owner chose it, so AGENTS.md should record the change. AGENTS.md still describes `edge_list` without the new field. The stash uses the same file names, so it will conflict when popped: decide discard or adapt.
* **M16. Brief structure [2/7].** Add a traceability table (requirement → decision → test → output row → step), an acceptance table (graph × algorithm → expected status), a risks section, a measurement section (B6) and a test strategy (M8).

### Disagreements to settle before drafting

1. Floyd-Warshall early stop: run every k and check at the end, or check after each complete k?
2. Optimization switches: keep them (limited to small graphs), or drop them and keep only the stats?
3. Shared reversal: a distinct `reversed_graph<G>` type, or a public single-source Bellman-Ford with a thin target wrapper?
4. Self-loops: vertex ids in `edge_list`, or a program-level "whole graph is negative_cycle" rule?
5. Floyd-Warshall rows: lvalue-only views, or copy the rows?

### Changes that remove a trade-off

* The implicit virtual source plus the O(1) negative-weight check: full cycle coverage costs nothing on W_2 to W_5.
* A distinct reversed-graph type: the reversal is shared and passing the wrong graph does not compile.
* Self-loop vertices passed into Floyd-Warshall's diagonal: the existing `dist[i][i] < 0` rule catches them with no special case.
* The vertices still changing in round n reused as BFS seeds: each requested distance is either right or marked undefined.
* Floyd-Warshall cost measured on the real graph by k-block timing or an n³ fit: an honest number within a budget of hours.

## Decisions after review round 1

The owner accepted the author's recommendations on the review findings (2026-11-21). Where these change a decision in "Agreed decisions" above, this section wins.

* **Direction: all case-study graphs are directed** (owner's reading of item 1 of the assignment). Direction is a required column in `config_part3.csv` with no default; every run prints `info,directed,yes|no`. Consequences to keep in mind: W_3 to W_5 were run undirected in Part 2, so Part 2 and Part 3 numbers on them are not comparable; whether directed W_1 has a negative cycle is unknown until it runs.
* **Runs on Azure.** Long runs (Floyd-Warshall on W_1 and W_2, all-pairs Bellman-Ford) go to an Azure VM through a Part 3 copy of `analysis/azure_part2.sh` (branch, harness and result names changed). Calibration first: time Floyd-Warshall at n = 1k, 2k, 4k, 8k and one Bellman-Ford run per graph, then fix run counts and the VM time budget with the owner before the long runs. Floyd-Warshall on W_3 (120 GB, weeks) and W_4/W_5 (do not fit) is reported as skipped, with the formula.
* **B1, self-loops:** `io::edge_list` keeps the ids of negative self-loop vertices (with their weights) next to the count. The algorithms take them as an optional input: Floyd-Warshall starts `dist[v][v] = min(0, w)`; the whole-graph cycle check reports such a vertex as a one-vertex cycle; Bellman-Ford marks it as reaching the target when its distance to the target is finite. Further io change, approved.
* **B2, endless walks:** with a negative cycle that affects them, `dist`, `next` and `pred` are unspecified except the witness. `path_to_target` and `tree_from` assert no such cycle; `path_to_target` stops after n steps; the program prints the status without calling a path function. Predecessor rules as in B2.
* **B3, whole-graph cycle check:** a separate graph-level function, run once per graph with its own time row. O(1) "no" when there is no negative weight and no negative self-loop; O(1) "yes" for an undirected graph with a negative edge. Implicit virtual source (every distance starts at 0, every vertex in the first frontier). Per-target Bellman-Ford reports only whether a cycle reaches the target.
* **B4, shared reversal:** `reversed(g)` returns a distinct `reversed_graph<G>` type that satisfies `weighted_graph`; the shared entry point accepts only that type (or an undirected graph). Negative test: a plain directed CSR is rejected.
* **Floyd-Warshall early stop (M4):** no early stop. Every k runs; the diagonal is checked once at the end.
* **Optimization switches (M9):** kept, with stats. The on/off comparison runs only on W_1, W_2 and synthetic graphs.
* **Floyd-Warshall rows (M7):** no-copy views, available only from a stored result (`tree_from` is lvalue-only; a test checks the rvalue call does not compile).
* **Per-source undefined distances (M11):** after a cycle reaching the target is found, a BFS on the reversed graph from the vertices still changing in the last round marks exactly the vertices whose distance is −∞, in O(n + m). This replaces the "marking −inf" non-goal. Each (source, target) row then gets `ok` or `negative_cycle` on its own.
* **Storage (M2):** Bellman-Ford and Floyd-Warshall run on CSR only; matrix rows are emitted as `skipped`.
* **Branching (M15):** Part 3 branches from `main`; recorded in AGENTS.md.

Still for the draft to settle (no owner decision needed, but must be written down): the float tolerance rule (M5), signatures and schemas (B5), the measurement protocol (B6), the test strategy (M8), round pseudocode (M9), where `reversed` lives (M10), Floyd-Warshall initialisation from CSR with parallel arcs and the row-skip rule (M12), the status table (M13), resumable runs (M14), traceability and acceptance tables (M16), `.gitignore` for `data/part3/`, and what to keep from the parked stash.

## Goals vs. Non-Goals

* **In-Scope Goals:** the direction flag; directed tests for the existing algorithms; `reversed`; Bellman-Ford with both optimizations and the any-cycle report; Floyd-Warshall with the in-place matrix and the predecessor matrix; the Part 3 benchmark, harness and config for the four case studies; docs (AGENTS.md module map, ONBOARDING.md §7).
* **Out-of-Scope Non-Goals:** run-time polymorphism in the library; strongly connected components or a directed diameter; Johnson's algorithm; SPFA with a FIFO queue; changes to Part 1 or Part 2 programs; multi-hour runs on the Mac without the owner's approval.

## Alternatives Weighed

* **`for_each_in_edge` in `weighted_graph`.** Rejected by author: it changes `concepts.hpp` and the owner's `matrix.hpp`, and makes every directed graph keep a reverse index whether or not Bellman-Ford runs.
* **Reuse `shortest_paths<W>` for Bellman-Ford, with `source = target`.** Rejected by author: `reconstruct_path` would return reversed paths with no compile error.
* **Throw on a negative cycle, like Dijkstra on a negative weight.** Rejected by author: it is a result of the run, and the case-study tables must show it.
* **Report only cycles that reach the target.** Rejected by owner in favour of "any negative cycle in the graph".
* **Copy each Floyd-Warshall row into a `shortest_paths<W>`.** Rejected by owner in favour of loosening `parent_tree` and returning a view.
* **One matrix per k in Floyd-Warshall.** Rejected: the assignment asks for the in-place version.
* **FIFO queue (SPFA) for the second optimization.** Rejected by author: different algorithm, different negative-cycle rule.

## Open questions (all answered in "Decisions after review round 1"; kept for the record)

1. **Direction of each case-study graph.** grafo_W_1 has 4,998 negative edges; read as undirected, each is a negative cycle by itself and every answer becomes "negative cycle". Is W_1 directed? Are W_2 to W_5 directed or undirected? The assignment leaves it to the user parameter.
2. **Floyd-Warshall at n = 25,000 for 10 runs** (estimate 2 to 4 days on the Mac): run fewer repetitions, run on Azure, or measure a smaller n and extrapolate?
3. **Floyd-Warshall on grafo_W_3 to W_5:** W_3 (120 GB) would fit the 128 GB VM that `azure_part2.sh` can create, but takes weeks; W_4 and W_5 fit nowhere. Report them as `no_memory` / skipped with the formula?
4. **The five disagreements and B1–B4** in the Analysis section.
5. **Branching:** record in AGENTS.md that Part 3 branches from `main`, as the owner chose?

## Key Decisions & Staging for Next Steps

* Decisions a–g above are agreed and carry into `/egm:egm-draft`.
* The draft should turn them into a detailed implementation plan, with one commit per item:
  1. directed flag and tests (done);
  2. Bellman-Ford;
  3. Floyd-Warshall;
  4. benchmark, harness and config;
  5. docs.

  It should also list the tests for each item and how to show each negative test fails without its check.
* After implementation: `/egm:egm-bootstrap` on `include/graph/`, then a drift report.
