# Part 3: directed graphs, Bellman-Ford and Floyd-Warshall

*   **Author:** Cerne17
*   **Status:** Reviewed (EGM review stages 5-9 done, 2026-11-22); ready for implementation
*   **Date:** 2026-11-21

--------------------------------------------------------------------------------

## Overview

Part 3 is the last part of the COS242 (Graph Theory, UFRJ) course project. It extends the existing C++23 header-only graph library in `include/graph/` with four capabilities the assignment (`trabalho-parte3.pdf`, section 2) requires:

1. directed weighted graphs, chosen by a user parameter, with each arc going from the first to the second vertex id of its line in the file;
2. BFS, DFS and Dijkstra on directed graphs;
3. Bellman-Ford from every vertex to one target vertex, returning distances, the shortest-path tree and whether the graph has a negative cycle, with the two optimizations taught in class;
4. Floyd-Warshall for all pairs, returning distances, the shortest-path tree from each vertex and whether the graph has a negative cycle, with the memory optimization taught in class.

It also adds a benchmark program, a harness and a configuration that produce the numbers for the assignment's four case studies on the five graphs in `data/part3/`, and a written report of at most 5 pages. All five graphs are run as directed (owner's decision).

| graph | n | arcs (m) | negative arcs |
|---|---|---|---|
| grafo_W_1 | 25,000 | 549,953 | 4,998 |
| grafo_W_2 | 25,000 | 824,964 | 0 |
| grafo_W_3 | 100,000 | 6,099,941 | 0 |
| grafo_W_4 | 1,000,000 | 30,999,978 | 0 |
| grafo_W_5 | 10,000,000 | 39,999,995 | 0 |

The work builds on the current architecture: compile-time policies (`directed` / `undirected`, `weighted<W>`), the concepts `graph_like` and `weighted_graph`, the CSR and matrix storages, and free algorithm templates in `gl::algorithms`. It does not redesign any of it. Part 1 and Part 2 programs and results stay as they are. Library changes to existing headers are limited to `io/edge_list.hpp` (Technical Plan §§1-2) and `algorithms/path.hpp` (§6).

The predecessor brief, with the review findings and the owner's decisions, is [part3.md](part3.md).

## The Problem

The library can already store a directed graph: `csr_storage` and `matrix_storage` take a `directed` policy, and BFS, DFS and the three Dijkstra versions follow out-arcs, so they already give directed answers. What it cannot do yet:

* **Choose direction at run time.** The Part 2 benchmark hard-codes `undirected`. The assignment wants the user to say whether a file is directed.
* **Handle negative weights.** Dijkstra rejects them with `std::invalid_argument`. grafo_W_1 has 4,998 negative arcs, so no shortest-path algorithm in the library accepts it.
* **Answer questions toward a target.** Every existing algorithm starts from a source. Bellman-Ford here must give, for every vertex, the distance and path *to* one vertex, and the existing path helper (`reconstruct_path`) would return those paths backwards if reused unchanged.
* **Answer all-pairs questions.** Nothing computes distances between every pair, and the library has no notion of "the tree rooted at each vertex" as a single result.
* **Say whether the graph has a negative cycle, anywhere.** The owner chose the literal reading of the assignment: a cycle anywhere counts, not only one that reaches the target. A single Bellman-Ford pass from the target cannot see cycles that do not reach it. Also, the file reader drops self-loops, and a negative self-loop is a negative cycle the algorithms would never see.

The case studies add a measurement problem. Floyd-Warshall needs `(n + 1)² × 12` bytes and about n³ steps: on the two 25,000-vertex graphs that is about 7.5 GB and an estimated 5 to 10 hours per run. The estimate's basis: each pivot moves about 24·n² bytes of memory (about 15 GB at n = 25,000), so a run moves about 375 TB, which takes 5 to 10 h at an assumed 10 to 20 GB/s of single-core bandwidth (the compute bound, 1 to 2 ns per cell, gives 4 to 9 h). On the 100,000-vertex graph and larger it does not fit or would take weeks. All-pairs Bellman-Ford would take about 1 to 2 days on W_3, 2 to 8 months on W_4 and 6 to 30 years on W_5 (n × an estimated pass of 0.5-2 s, 5-20 s and 20-100 s, of the order of a Dijkstra run in Part 2). These times are estimated, not measured; calibration on the VM replaces them. The numbers in the report must therefore come from runs on an Azure VM, after a short calibration, with any extrapolation labelled as such.

Proposed solution: add a direction flag to a new Part 3 benchmark that picks the compile-time type; add a reversed-graph type and run Bellman-Ford toward the target as an ordinary single-source pass on it, with both optimizations; add a separate whole-graph negative-cycle check run once per graph; add Floyd-Warshall with one in-place distance matrix and a predecessor matrix whose rows act as shortest-path trees; and drive the case studies from a Part 3 harness on Azure.

## Goals

Terms such as *step*, *answer run* and *scaled weight* are defined in the [Glossary](#glossary).

1. **Direction at run time.** `benchmark_part3` requires `--directed yes|no` and prints it on every run. All four combinations of CSR / matrix × directed / undirected compile and load. Hand-checked tests (already committed in `879a495`) show direction changing BFS, DFS and Dijkstra results, with CSR and matrix agreeing.
2. **Bellman-Ford toward a target.** For every vertex v, the result gives the shortest distance from v to the target, the next vertex on that path, and the path itself through a helper that lists v first. Distances are correct with negative weights, checked against independent references (Dijkstra on non-negative graphs, graphs built from potentials, Floyd-Warshall).
3. **Both optimizations, shown to prune.** Three variants can be chosen: the textbook version (every arc, all n rounds), optimization 1 alone (stop after a round with no change), and optimizations 1 and 2 (also relax only around vertices that changed in the previous round). Tests show, through counters of rounds and relaxations, that textbook ≥ stop_when_stable ≥ frontier on every test graph, and strictly less work on a star fixture, with the same distances (`next` may differ between paths of equal weight; stop_when_stable and frontier also take the same number of rounds).
4. **Negative cycles reported, not thrown.** A graph-level check says whether a negative cycle exists anywhere, including one-arc cycles from negative self-loops, and returns one checked cycle with its weight as evidence. Bellman-Ford says, for each vertex, whether its distance to the target is undefined (−∞) because a negative cycle lies on the way; Floyd-Warshall says the same for each pair. No path helper can loop forever on a result with a negative cycle.
5. **Floyd-Warshall with the memory optimization.** One distance matrix updated in place, plus a predecessor matrix. Row i acts as the shortest-path tree from i and works with `reconstruct_path` (after the `parent_tree` change in §6) without copying. Results match Bellman-Ford and Dijkstra on every vertex pair of the random test graphs, and every reconstructed path's weight equals its distance.
6. **Case-study numbers, reproducible.** One harness run produces, for each graph in `data/part3/`, the assignment's four case studies: (1) Bellman-Ford distances from 10, 20 and 30 to 100; (2) Floyd-Warshall distances among {10, 20, 30, 40, 50} where it fits in memory; (3) the mean and median time of each algorithm over up to 10 runs, without disk I/O, in seconds, with the number of runs; (4) all-pairs Bellman-Ford against Floyd-Warshall. Every row that is not `ok` gives its status from Technical Plan §9 and the reason.
7. **Quality bar unchanged.** `make test` passes with the existing flags on macOS after every commit, and on Linux on the VM before any measurement; `make test-part3` passes after the commits that touch the program or the harness. No new warnings in the debug build or the `-DNDEBUG` release benchmark. Every new header has a test file, every new `requires` clause or concept has a negative test, and expected values come from hand computation or an independent reference, never from the code under test.
8. **Bounded measurement.** (a) The repeated timing of each step stays within its time budget (default 600 s), except that run 1 always runs, and reports how many runs it measured. (b) Answer runs (Bellman-Ford toward 100, Floyd-Warshall among the set) always run once if they fit in memory. (c) Azure: one VM of the smallest size that fits, deleted after the results are fetched. (d) A run that dies loses at most its unfinished launch; the only multi-hour launch is a single Floyd-Warshall run. The rules are in Technical Plan §8 and §10.

## Non-Goals

* **Run-time polymorphism in the library.** Direction stays a compile-time policy; only the program maps the flag to a type.
* **Changes to `concepts.hpp` or `storage/matrix.hpp`.** No `for_each_in_edge`, no reverse index inside the storages.
* **Bellman-Ford and Floyd-Warshall on the matrix storage in the benchmark.** The assignment makes the matrix optional; matrix rows are reported as `skipped`. The algorithms still accept any `weighted_graph`.
* **Floyd-Warshall on grafo_W_3 to W_5.** Reported as `no_memory`, with the memory need (120 GB, 12 TB, 1.2 PB) and a c·n³ time estimate.
* **Multi-threaded Floyd-Warshall.** It would make the comparison with single-threaded Bellman-Ford unfair.
* **Other algorithms:** Johnson's algorithm, SPFA with a FIFO queue, strongly connected components, a directed diameter. Part 1's components, distance and degree statistics are not adapted to digraphs.
* **Changes to Part 1 or Part 2 programs and results.** `benchmark_weighted`, `harness_weighted.py`, `config_weighted.csv` and their outputs stay as they are (`harness_part3.py` imports helpers from `harness_weighted.py` without changing it).
* **Comparing Part 3 numbers with Part 2 numbers on W_3 to W_5.** Part 2 ran them undirected; Part 3 runs them directed.

## Decisions and Open Questions

### Decision log

Decisions made while drafting and reviewing this design (2026-11-21 and 2026-11-22), by the owner. Where this design differs from [part3.md](part3.md), this log and the design win.

* **Fixed-point weights.** The Part 3 program parses each weight token as text: at most 2 digits after the point, integer built as sign × (integer part × 100 + fractional digits), never `round(w * 100)`; more decimals is a `path:line: ...` `std::runtime_error`. One named constant, the number of decimals (2); the scale (100) comes from it. Distances are printed unscaled with exactly 2 decimals. The library still compares plain doubles exactly for users who do not scale; the report states the convention. Details in Technical Plan §1.
* **Parser location.** A parser hook in `read_edge_list`, and a generic `io::fixed_point_weight` parser in the library (§1).
* **Time budget.** Repeated timing shares a 600 s budget per step, replacing a 20 s per-run limit (Alternative T); answer runs run once.
* **Reconciliation with the brief:**
  * `negative_self_loop_edges` replaces the counter;
  * `--directed` has no default;
  * `path` rows carry distances (no `dist` row); `cycle`, `check`, `progress`, `calib`, `fit` and `done` rows are added, and the harness writes the estimates;
  * the reversed graph lives in `storage/reversed.hpp`;
  * `path_to_target` returns empty in O(1) instead of asserting;
  * calibration sizes double from 500 within the step budget;
  * the optimization comparison is a `bf` option run on W_1 and W_2.
* **Review, Stage 7 round 1** ([round 1 resolution report](part3_review/goldfish_review_report_round1.md), accepted as a batch): negative-self-loop vertices seed the −∞ marking after the rounds end; Floyd-Warshall marks −∞ per pair (`minus_infinity`); one negative-cycle witness, checked and with its weight; Floyd-Warshall's time is its measured run; `progress` rows and a watchdog replace a fixed timeout; the status table was rewritten; a Risks section was added.
* **Review, Stage 7 round 2** ([round 2 resolution report](part3_review/goldfish_review_report_round2.md), accepted as a batch): synchronous Bellman-Ford rounds; the Floyd-Warshall time estimate raised to 5 to 10 h with its basis, and tiling rejected (Alternative Y); `tree_from` valid per cell; the Floyd-Warshall cross-check moved into the harness; all-pairs on W_1 and W_2 measured in full when it fits the budget; a 16 GB VM; `calib` and `fit` rows and a metric list.
* **Review, Stage 7 round 3** ([round 3 resolution report](part3_review/goldfish_review_report_stage7_round3.md), accepted as a batch): `reversed` and `bellman_ford` reject an already reversed graph; cross-checks run as separate `bf_check` launches with `--runs 1`; all-pairs estimates per graph; §10 split into one item per requirement; `pred(i, i)` reset after Floyd-Warshall; watchdog floor 2 × budget.
* **Review, Stage 8 and Stage 9 round 1** ([Stage 9 resolution report](part3_review/goldfish_review_report.md), accepted as a batch; the review loop then stopped by the owner's decision): one Floyd-Warshall kernel and no bandwidth model (Alternative Z); one output file per launch instead of attempt ids, and a fixed 20-minute watchdog; `variants_agree` dropped; the Detailed Implementation Plan rewritten from §§1-11.

Resolved in the Technical Plan: the measurement protocol (§8), where `reversed` lives (§3), the parked prototype (§11).

### Open questions

* **VM time budget.** The owner approves the calibration's estimate before the full run. The two single Floyd-Warshall runs (n = 25,000) are estimated at 5 to 10 hours each (see [The Problem](#the-problem)), not measured.

## Critical User Journeys (CUJs)

### CUJ 1: Bellman-Ford case study on a directed graph with negative weights

*   **Given**: `data/part3/grafo_W_1.txt.gz` (n = 25,000, 4,998 negative arcs) and a `config_part3.csv` row that marks it directed.
*   **When**: the owner runs the Part 3 harness for that graph.
*   **Then**: `paths_part3.csv` has one row for each of 10 → 100, 20 → 100 and 30 → 100, with distance, number of arcs and path, or with status `unreachable` or `negative_cycle` for that row alone; `results_part3.csv` says whether the graph has a negative cycle anywhere, with one checked cycle and its weight as evidence, and the mean and median Bellman-Ford time over up to 10 runs, with the sample count.

### CUJ 2: Floyd-Warshall case study where it fits

*   **Given**: grafo_W_1 or grafo_W_2 (n = 25,000) and an Azure VM with enough memory for 7.5 GB of matrices plus the graph.
*   **When**: the harness runs the Floyd-Warshall step after the calibration was approved.
*   **Then**: Floyd-Warshall runs once (estimated 5 to 10 h), printing progress rows the owner can follow. `paths_part3.csv` has the 20 ordered pairs among {10, 20, 30, 40, 50}, each `ok` (distance and path), `unreachable` or, if the graph has a negative cycle that affects that pair, `negative_cycle`; the other pairs keep their exact distances. Afterwards the harness cross-checks those rows against five short `bf` launches toward the set vertices. `results_part3.csv` has the measured run as the Floyd-Warshall time (status `fewer_runs`, 1 sample) and the negative-cycle answer, which agrees with the graph-level check.

### CUJ 3: Floyd-Warshall where it does not fit

*   **Given**: grafo_W_3, W_4 or W_5.
*   **When**: the harness reaches the Floyd-Warshall step.
*   **Then**: the binary is not launched for Floyd-Warshall; `results_part3.csv` has a row with status `no_memory`, the computed memory need and a c·n³ time estimate, so the report can explain the gap.

### CUJ 4: All-pairs Bellman-Ford against Floyd-Warshall

*   **Given**: any case-study graph.
*   **When**: the harness runs the all-pairs step.
*   **Then**: all-pairs Bellman-Ford is n single-target passes, timed toward targets in a fixed-seed order; at least one pass is always measured. If, after the first passes, one reversal + n × the longest pass fits the step budget (possible on W_1 and W_2), all n passes run and the measured total is reported (`ok`). Otherwise (estimated on W_3 to W_5) the passes stop at the budget or after k targets, and "one reversal + n × mean pass time" is reported with status `extrapolated`, the spread of the pass times, the number of targets measured and the formula. The Floyd-Warshall side is its measured run on W_1 and W_2, and the c·n³ estimate (`extrapolated`, with the `no_memory` caveat) on W_3 to W_5. The report can say which approach was faster and why.

### CUJ 5: A library user asks for a path when a negative cycle is in the way

*   **Given**: a Bellman-Ford result where a negative cycle reaches the target from vertex v.
*   **When**: the user asks for v's path.
*   **Then**: the result already marks v's distance as −∞, so the path helper checks that first and returns an empty path at once, in every build (debug and release), without walking. As a last guard, a walk that still meets a repeated vertex stops after n steps and returns an empty path too.

### CUJ 6: Calibrating before a long run

*   **Given**: a fresh Azure VM with the Part 3 branch built and `make test` passing.
*   **When**: the owner runs the calibration step.
*   **Then**: the harness prints measured Floyd-Warshall times at small n (sizes doubling from 500; the next size runs only if 8 × the last run's time fits the step budget), a fitted c·n³ estimate for each case-study graph, and one Bellman-Ford pass per graph. Calibration only estimates: it gives the VM time for the full run, dominated by the two single Floyd-Warshall runs on W_1 and W_2, and the estimates for graphs that are not run; the run-time budget rule decides how often each step repeats. The owner approves the VM-time estimate before the full run starts. During the run, Floyd-Warshall's progress rows replace the estimate with a live ETA (see [Risks & Mitigations](#risks--mitigations)).

## Technical Plan (Architecture)

Codes such as B1 or M6 in the headings below refer to the review findings recorded in [part3.md](part3.md).

### Data flow

```text
 data/part3/grafo_W_k.txt.gz         config_part3.csv (graph, directed, targets, ...)
            |                                    |
            v                                    v
 analysis/harness_part3.py: decompress once per graph (plain_text, from harness_weighted.py)
            |
            +--- launches ---> build/benchmark_part3 <graph.txt> --directed yes|no <mode> ...
                                                 |
   gl::io::read_edge_list<weighted<double>>(path, io::fixed_point_weight<double>{2})   (text, <= 2 decimals, x100)
                                                 |
                       edge_list { n, edges (scaled), negative_self_loop_edges }
                                                 |
                      gl::csr_graph<directed | undirected, weighted<double>>::from_edges
                                                 |
          +--------------------------+-----------+--------------+----------------------------+
          |                          |                          |                            |
 find_negative_cycle(g, loops)   gl::reversed(g)            floyd_warshall(g, loops)    bfs / dfs / dijkstra_*
 (once per graph)                    |                          |                       (unchanged)
          |                 bellman_ford_on_reversed(r, t, ...)  all_pairs: dist, pred, minus_infinity, tree_from(i)
          |                          |                          |
          +--------- rows on stdout, flushed after every row: info / time / progress / path / cycle / check / done
                                                 |
                  harness: data/results_part3.csv (+ estimate rows), data/paths_part3.csv  -> analysis/report_part3.py (tables, times in seconds)
```

All long runs happen on one Azure VM through `analysis/azure_part3.sh` (adapted from `azure_part2.sh`, §10). The library stays header-only and compile-time; only the program turns `--directed` into a type.

### Glossary

* **Toward-target tree (Bellman-Ford):** rooted at the target t. `next[v]` is the vertex after v on v's shortest path to t. Paths are listed v first, t last.
* **From-source tree (Floyd-Warshall row i, BFS, DFS, Dijkstra):** rooted at the source. `parent[v]` is the vertex before v. Paths are listed source first.
* **Scaled weight:** a file weight × 100, held exactly as an integer-valued `double`. Only the Part 3 program chooses to scale; the library offers a generic fixed-point parser but its algorithms do not know whether weights are scaled.
* **Step:** one timed quantity in one launch of the binary: (graph, mode, algorithm or variant). Its repeated runs share one time budget. The `reverse` and `bellman_ford_pass` rows of one `bf` run belong to the same step; the reported Bellman-Ford time is their sum, with both parts shown.
* **Answer run:** the first run of a step whose result a case study asks for (Bellman-Ford toward 100, Floyd-Warshall among the set). It always runs, if it fits in memory.

### 1. Reading weights as text: `read_edge_list` parser hook

An owner-approved change to io. New overload in `include/graph/io/edge_list.hpp`:

```cpp
// Like read_edge_list<W>(path), but the weight field is handed to
// parse_weight as its text (the token between whitespace). parse_weight
// returns the weight, or throws std::invalid_argument with a message; the
// reader rethrows it as std::runtime_error "path:line: <message>".
// W must be given explicitly: it appears only in the return type.
template<weight_policy W, typename ParseWeight>
  requires W::is_weighted &&
           std::is_invocable_r_v<typename W::weight_type, ParseWeight&, std::string_view>
edge_list<W> read_edge_list(const std::filesystem::path& path, ParseWeight parse_weight);
```

The existing `read_edge_list<W>(path)` becomes a call to the shared body with a parser that reads the token with stream extraction, so its behaviour and error messages stay the same: the existing io tests, which assert the messages (`tests/test_edge_list.cpp`), must still pass.

The fixed-point parser is generic and lives in the library (owner's decision), new header `include/graph/io/fixed_point.hpp`, namespace `gl::io`:

```cpp
// Parses a decimal token with at most `decimals` digits after the point into
// the exact integer sign * (integer part * 10^decimals + fractional digits),
// as a W. Never multiplies a parsed floating-point value. Accepts an optional
// sign, at least one digit, an optional point followed by 1..decimals digits.
// Throws std::invalid_argument for anything else: more decimals, exponents,
// empty digits, trailing text, or a magnitude above 2^digits(W) (where W stops
// holding every integer exactly).
template<std::floating_point W>
struct fixed_point_weight
{
  unsigned decimals = 2;                 // precondition (assert): 0..9
  W operator()(std::string_view token) const;
  W scale() const;                       // 10^decimals, exact
};
```

The Part 3 program keeps one named constant, the number of decimals (2); the scale (100) comes from `fixed_point_weight::scale()`, so the parser and the printing cannot disagree. Error example: `grafo.txt:7: weight has more than 2 decimals: 1.234`.

Exactness of sums: they stay exact while (n − 1) × max |scaled w| ≤ 2^53 (about 9 × 10^15). W_1 and W_2 were scanned: their weights lie in [−5.00, 10.00], so |scaled w| ≤ 1,000 and sums stay below 2.5 × 10^7 (and below 10^10 on W_5 if its weights are in the same range); W_3 to W_5 are checked at run time: the program prints `info,max_abs_weight` and exits with an error if (n − 1) × max |scaled w| > 2^53. Floyd-Warshall cells affected by a negative cycle can grow past the bound; they are reported as `negative_cycle` and never printed.

### 2. Negative self-loops kept (B1)

An owner-approved change to io. `io::edge_list<W>` replaces the committed counter `negative_self_loops` with `std::vector<input_edge_t<W>> negative_self_loop_edges` (the `v v w` lines with w < 0, in file order; always empty for unweighted policies). `skipped_self_loops` keeps counting all loops. The algorithms that need them take an optional `std::span<const weighted_edge<W>> negative_loops` argument; the program passes the list through. For a weighted policy `input_edge_t<weighted<X>>` is `weighted_edge<X>` (`storage/input_edge.hpp`), so the vector converts to the span without copying. The counter commit was never pushed, so it is replaced, not kept beside the vector.

### 3. Reversed graph type (B4, M10)

New header `include/graph/storage/reversed.hpp`, namespace `gl` (it is a data structure, like the storages; the factory sits next to its type):

```cpp
// A directed graph with every arc of G turned around (u -> v of weight w
// becomes v -> u of weight w). A distinct type, so code that needs "the
// reversal of g" cannot be given g itself by mistake. Built from G's arcs
// through csr_graph::from_edges: O(n + m log d_max) time (the per-vertex sort
// in from_edges) and a temporary copy of the
// m arcs, then the CSR alone (n + 2 offsets, m targets, m weights).
template<weighted_graph G>
  requires G::direction_type::is_directed
class reversed_graph
{
private:
  csr_graph<directed, typename G::weighting_type> arcs_;
  explicit reversed_graph(csr_graph<directed, typename G::weighting_type> arcs);

public:
  using direction_type = directed;
  using weighting_type = typename G::weighting_type;
  using weight_type = typename G::weight_type;

  vertex_id vertex_count() const;
  template<typename F> void for_each_neighbor(vertex_id u, F&& f) const;
  template<typename F> void for_each_edge(vertex_id u, F&& f) const;   // f(target, weight)
  bool has_negative_weight() const;

  template<weighted_graph H> requires H::direction_type::is_directed && (!is_reversed_graph<H>)
  friend reversed_graph<H> reversed(const H& g);
};

// True only for reversed_graph specializations (declared, with a forward
// declaration of reversed_graph, before the class in the real header).
template<typename G> constexpr bool is_reversed_graph = /* ... */;

// Rejects a graph that is already reversed: reversing it back would silently
// turn distances to t into distances from t.
template<weighted_graph G> requires G::direction_type::is_directed && (!is_reversed_graph<G>)
reversed_graph<G> reversed(const G& g);
```

`reversed` on an undirected graph does not compile: an undirected graph is its own reversal, and Bellman-Ford uses it directly. Tests: `static_assert(weighted_graph<reversed_graph<…>>)`; a local concept shows `reversed(undirected_g)` is rejected; another shows `bellman_ford_on_reversed` rejects a plain directed CSR; two more show `reversed(reversed(g))` and `bellman_ford(reversed(g), t)` are rejected.

### 4. Bellman-Ford toward one target

New header `include/graph/algorithms/bellman_ford.hpp`, namespace `gl::algorithms`.

```cpp
template<std::floating_point W>
struct target_paths
{
  vertex_id target = 0;
  std::vector<W> dist;          // size n + 1. dist[v]: shortest v -> target;
                                // +inf if v cannot reach target; -inf if a
                                // negative cycle lies on some v -> target walk
  std::vector<vertex_id> next;  // size n + 1. vertex after v on its path;
                                // 0 for target, +inf and -inf vertices
  bool cycle_reaches_target = false;  // some dist is -inf
};

// The three variants: the textbook version (every arc, all n rounds),
// optimization 1 (stop after a round with no change), and optimizations 1 and 2
// (also relax only around vertices that changed in the previous round; an
// empty frontier ends the loop).
enum class bellman_ford_variant { textbook, stop_when_stable, frontier };

struct bellman_ford_options
{
  bellman_ford_variant variant = bellman_ford_variant::frontier;
};

struct bellman_ford_stats
{
  std::uint32_t rounds = 0;      // rounds started
  edge_index relaxations = 0;    // arcs examined (not only the ones that improved)
};

// rev: gl::reversed(g) for a directed g, or g itself when undirected.
template<weighted_graph R>
  requires reverse_input<R>  // reversed_graph<G> or an undirected graph
target_paths<typename R::weight_type> bellman_ford_on_reversed(
  const R& rev, vertex_id target,
  std::span<const weighted_edge<typename R::weight_type>> negative_loops = {},
  bellman_ford_options options = {}, bellman_ford_stats* stats = nullptr);

// Convenience: builds the reversal itself when g is directed. A reversal goes to
// bellman_ford_on_reversed instead.
template<weighted_graph G>
  requires (!is_reversed_graph<G>)
target_paths<typename G::weight_type> bellman_ford(
  const G& g, vertex_id target,
  std::span<const weighted_edge<typename G::weight_type>> negative_loops = {},
  bellman_ford_options options = {}, bellman_ford_stats* stats = nullptr);

// v first, target last. Empty, in O(1) and in every build, when dist[v] is
// +inf or -inf (checked first, so a target on a negative cycle gives empty);
// otherwise {target} when v == target. As a last guard, a walk longer than n
// steps stops and returns empty.
template<std::floating_point W>
std::vector<vertex_id> path_to_target(const target_paths<W>& tree, vertex_id v);
```

`reverse_input<R>` is a small concept in the same header (a `reversed_graph` specialization, or `!R::direction_type::is_directed`); it does not touch `concepts.hpp`.

One round, as pseudocode. Rounds are synchronous: each round reads `prev`, the distances at the end of the previous round, and writes `dist`; after the round, the changed entries are copied into `prev` (O(changed), one extra array of n distances). So stop_when_stable and frontier compute the same distances in the same rounds, and frontier relaxes a subset of the arcs: textbook ≥ stop_when_stable ≥ frontier in both counters.

```text
dist[*] = +inf, next[*] = 0, dist[target] = 0; prev = dist
frontier = [target]
for round = 1 .. n:
    stats.rounds = round; changed_this_round = []
    if variant == frontier: scan = frontier
    else:                   scan = every x with prev[x] < +inf, in id order
    for x in scan:
        for each arc x -> u of weight w in rev:          # u -> x in the real graph
            stats.relaxations += 1
            if prev[x] + w < dist[u]:
                dist[u] = prev[x] + w; next[u] = x
                add u to changed_this_round (once per round)
    for u in changed_this_round: prev[u] = dist[u]
    if changed_this_round is empty and variant != textbook: stop
    frontier = changed_this_round
changed_in_round_n = changed_this_round if the loop ran round n, else []
```

The textbook variant runs all n rounds even after nothing changes (used by the pruning tests).

After the rounds end, early or at round n:

1. **Seeds:** the vertices in `changed_in_round_n`, plus every negative-self-loop vertex v with `dist[v] < +inf` (a self-loop is not an arc of `rev`, so the rounds never see it). No seeds means no negative cycle reaches the target.
2. **Mark −∞ exactly:** BFS in `rev` from the seeds. A round-n vertex has true distance −∞, and every negative cycle that reaches the target has at least one such vertex (otherwise all its arcs would satisfy dist[u] ≤ dist[x] + w after round n and its weight would be ≥ 0). A self-loop vertex that reaches the target has distance −∞ by definition. Every vertex the BFS reaches can reach a seed, so it gets `dist = -inf`, `next = 0`. O(n + m), once. Unmarked vertices keep valid distances and loop-free `next` chains. If there were seeds, `cycle_reaches_target = true`.

Cost: O(n·m) worst case per pass; O(m) per round on the frontier. On a directed graph `bellman_ford` adds the reversal, O(n + m log d_max).

### 5. Whole-graph negative-cycle check (B3)

In `algorithms/bellman_ford.hpp`:

```cpp
struct negative_cycle_report
{
  bool found = false;
  std::vector<vertex_id> cycle;  // the only witness in the library: arc order,
                                 // v0 not repeated; empty if !found
};

// Any negative cycle anywhere in g, including the negative self-loops passed in.
// Run once per graph. O(1) when !g.has_negative_weight() and negative_loops is
// empty (answer: none). O(1) when negative_loops is non-empty (answer: found,
// cycle {v}). O(m) for an undirected g with a negative edge (answer: found,
// cycle {u, v}). Otherwise Bellman-Ford rounds (frontier variant) on g itself
// (no reversal: reversing keeps cycle weights) from an implicit virtual source:
// every dist starts at 0 and every vertex is in the first frontier; a change in
// round n means a cycle, and the witness comes from walking the predecessor
// pointers n steps from a vertex changed in round n, then collecting the cycle
// and reversing it into arc order.
template<weighted_graph G>
negative_cycle_report find_negative_cycle(
  const G& g,
  std::span<const weighted_edge<typename G::weight_type>> negative_loops = {},
  bellman_ford_stats* stats = nullptr);
```

Bellman-Ford's per-target result carries only `cycle_reaches_target`; whether a cycle exists anywhere is this check, reported once per graph. In a full all-pairs run (possible on W_1 and W_2) the check is redundant: the union of the per-target flags is the same answer.

### 6. `parent_tree` loosened, and a bounded `reconstruct_path` (M6)

In `include/graph/algorithms/path.hpp`:

```cpp
// Anything rooted at a source with a 1-indexed parent array: parent[source]
// == 0, parent[v] == 0 for unreached v.
template<typename T>
concept parent_tree = requires(const T& t) {
  { t.source } -> std::convertible_to<vertex_id>;
  requires std::ranges::random_access_range<decltype((t.parent))>;
  requires std::ranges::sized_range<decltype((t.parent))>;
  requires std::same_as<std::ranges::range_value_t<decltype((t.parent))>, vertex_id>;
};
```

`search_tree` and `shortest_paths` still qualify; `all_pairs<W>::row_tree`, whose `parent` is a `std::span<const vertex_id>`, now qualifies too. `reconstruct_path` also stops after `parent.size()` steps and returns empty instead of looping, so a corrupt tree cannot hang a program. Negative tests: `std::list<vertex_id>` (not random access), a `std::vector<int>` (wrong value type), a type without `source`, and `static_assert(!parent_tree<target_paths<double>>)`. Goes in its own commit before Floyd-Warshall.

### 7. Floyd-Warshall with the memory optimization

New header `include/graph/algorithms/floyd_warshall.hpp`, namespace `gl::algorithms`.

```cpp
template<std::floating_point W>
class all_pairs
{
private:
  vertex_id n_ = 0;
  std::vector<W> dist_;          // (n + 1)^2, cell (i, j) at i * (n + 1) + j (std::size_t); row/col 0 unused
  std::vector<vertex_id> pred_;  // same layout
  std::vector<vertex_id> on_cycle_;  // every k with dist(k, k) < 0 after the run

public:
  struct row_tree
  {
    vertex_id source;
    std::span<const vertex_id> parent;  // size n + 1: pred row of source
    std::span<const W> dist;            // size n + 1
  };

  vertex_id vertex_count() const;
  W dist(vertex_id i, vertex_id j) const;            // +inf if unreachable; meaningful only if !minus_infinity(i, j)
  bool minus_infinity(vertex_id i, vertex_id j) const;  // a negative cycle lies on some i -> j walk; O(on_cycle_.size())
  bool negative_cycle() const;                       // on_cycle_ is non-empty
  row_tree tree_from(vertex_id i) const&;            // no copy; reconstruct_path(tree_from(i), j) is valid when !minus_infinity(i, j)
  row_tree tree_from(vertex_id i) const&& = delete;  // no view into a temporary
  std::size_t bytes() const;                         // (n + 1)^2 * (sizeof(W) + sizeof(vertex_id))
};

struct no_op_pivot { void operator()(vertex_id, vertex_id) const noexcept {} };

// on_pivot(k, rows_not_skipped) is called after each k; the default does nothing.
template<weighted_graph G, typename OnPivot = no_op_pivot>
all_pairs<typename G::weight_type> floyd_warshall(
  const G& g, std::span<const weighted_edge<typename G::weight_type>> negative_loops = {},
  OnPivot on_pivot = {});
```

Algorithm, written out:

```text
init: dist(i, j) = +inf, pred(i, j) = 0 for all i, j
      dist(i, i) = 0                                     (pred(i, i) = 0)
      for each arc i -> j of weight w (for_each_edge):   keep the lightest of parallel arcs
          if w < dist(i, j): dist(i, j) = w, pred(i, j) = i
      for each negative self-loop (v, w): dist(v, v) = min(dist(v, v), w)
for k = 1 .. n:                                           every k runs: no early stop
    for i = 1 .. n, i != k:                               (row k never improves through k: see below)
        if dist(i, k) == +inf: skip row i                 (no path i -> k: nothing to gain)
        for j = 1 .. n:
            if dist(i, k) + dist(k, j) < dist(i, j):
                dist(i, j) = dist(i, k) + dist(k, j); pred(i, j) = pred(k, j)
    on_pivot(k, rows not skipped)
pred(i, i) = 0 for every i                                (a negative cycle may have overwritten it)
on_cycle = every k with dist(k, k) < 0
minus_infinity(i, j) = some k in on_cycle with dist(i, k) < +inf and dist(k, j) < +inf
```

Implementation note: one kernel. For each k, `dist(i, k)` and pointers to rows i and k of both matrices are hoisted out of the j loop (with i ≠ k the rows never alias); the j loop runs over contiguous memory and writes a cell only when it improves, which is the class algorithm written out and writes less memory late in a run. Vectorization is not required.

**Memory optimization.** One matrix of each kind, updated in place for every k (the memory optimization from class): row k and column k do not change during step k unless dist(k, k) < 0.

**Correctness with negative cycles.** The pairs with `minus_infinity(i, j)` have no defined distance: each negative cycle's highest-numbered vertex ends with dist(k, k) < 0, and a negative closed walk through k means k lies on or reaches a negative cycle, so the test above marks exactly the pairs joined through a negative cycle. Every other pair, and its predecessor chain, only ever combined values of unaffected pairs, so it is exact; the predecessor chain of an unaffected pair passes only through unaffected pairs. Skipping row i = k is safe: if dist(k, k) ≥ 0, row k does not change in step k; if dist(k, k) < 0, the cells it would update are already affected, and finiteness does not change. Affected cells may reach −inf; then −inf + +inf gives NaN, but NaN < x is false, so NaN is never stored, and the finiteness that `minus_infinity` reads is unchanged. The program reconstructs a path only for `ok` pairs.

**Memory footprint.** (n + 1)² × 12 bytes, 7.5 GB at n = 25,000. Floyd-Warshall builds from CSR in the program and the CSR is kept (its 6.6 MB to 10 MB is small next to 7.5 GB).

### 8. Benchmark program `app/benchmark_part3.cpp`

```text
benchmark_part3 <graph> --directed yes|no <mode> [options]
  modes:  calibrate | cycle | bf | fw | allpairs
  --target T            bf: the target (100)
  --sources a,b,c       bf: vertices whose path to T is printed (10,20,30)
  --set a,b,c,...       fw: vertices whose pairwise paths are printed (10,20,30,40,50)
  --runs N              repetitions for timing (10)
  --budget-seconds S    time budget per step (600)
  --optimizations V,..  bf: also time these variants (textbook|stop_when_stable|frontier)
  --progress-every K    fw: print a progress row every K pivots (250)
  --k N --seed S        allpairs: sample size (100) and seed (2025)
  --no-bf-pass          calibrate: skip the Bellman-Ford pass
```

* `--directed` is required; the program maps it to `csr_graph<directed, …>` or `csr_graph<undirected, …>`. The matrix storage is not used by Part 3 modes; the harness writes `skipped` rows for it.
* Weights are read with `read_edge_list<gl::weighted<double>>(path, io::fixed_point_weight<double>{weight_decimals})` (§1); `weight_decimals = 2` is the one named constant and the scale (100) is derived from it. Distances are printed as `dist / scale` with exactly 2 decimals; `inf` and `-inf` print as such.
* **Budget rule (decides repetitions at run time):** run 1 always runs. Another run starts while runs < `--runs` and elapsed (including run 1) + the longest run so far ≤ `--budget-seconds`.
* Timing (steady clock, microseconds, after the graph is loaded; disk I/O never timed):
  * `bf`: one run = `reversed(g)` (row `reverse`) + one `bellman_ford_on_reversed` toward T (row `bellman_ford_pass`); the reported Bellman-Ford time is their sum. One run answers every source. Nothing is reused between runs except the loaded graph. With `--optimizations`, each listed variant is timed the same way, as its own step, with rows `bellman_ford_pass_<variant>` and `info` rows `rounds_<variant>` and `relaxations_<variant>`. With `--runs 1` the launch makes only the answer run; the harness's cross-checks use it.
  * `fw`: one run = `floyd_warshall(g)`, including allocating and initialising the matrices, with a flushed `progress` row every K pivots and at least every 60 s. Same budget rule; at n = 25,000 the first run alone exceeds the budget, so only the answer run is measured. Then, for each ordered pair of `--set`, a `path` row with status `ok`, `unreachable` or `negative_cycle` (`minus_infinity` is called only for these pairs). The cross-check against Bellman-Ford is done by the harness (§10), outside this launch.
  * `cycle`: `find_negative_cycle(g, loops)`, same budget rule. Before printing, the program checks the witness: each consecutive pair (and the closing pair) must be an arc of g or a negative self-loop, and the sum of the lightest such arcs must be negative.
  * `allpairs`: one reversal, then single-target passes toward targets in a fixed-seed order, each timed with its target. If, after the first passes, reversal + n × the longest pass fits the budget, all n targets run (a full measurement); otherwise the passes stop when the next would not fit, or after k targets.
  * `calibrate`: Floyd-Warshall on random directed graphs with the same average out-degree as the input, at sizes doubling from 500; the next size runs only if 8 × the last run's time fits the budget. One Bellman-Ford pass on the real graph (`--no-bf-pass` skips it; the harness passes it when a `bf` answer run is already recorded). Prints the c·n³ fit. Calibration only estimates; the budget rule above decides. It prints a `progress` row at least every 60 s.
* Output: one CSV row per line, flushed after every row. The last row of a launch is `done,<mode>`.

| row kind | columns | when |
|---|---|---|
| `info` | `info,<metric>,<value>` | metadata: `vertices`, `edges`, `directed`, `skipped_self_loops`, `negative_self_loops` (the size of `negative_self_loop_edges`), `scale`, `seed`, `k`, `budget_seconds`, `max_abs_weight`, `memory_bytes` (`fw`), `rounds`, `relaxations` (and `_<variant>` forms) |
| `time` | `time,<algorithm>,<run>,<micros>[,<param>]` | one per measured run; `algorithm` in `reverse`, `bellman_ford_pass`, `bellman_ford_pass_<variant>`, `floyd_warshall`, `negative_cycle_check`, `allpairs_pass`; `param` is the target for `allpairs_pass`, empty otherwise; no status |
| `calib` | `calib,floyd_warshall,<n>,<arcs>,<run>,<micros>` | one per calibration run |
| `fit` | `fit,<model>,<c>,<points>` | calibration model `floyd_warshall_n3` (seconds per n³) |
| `progress` | `progress,<algorithm>,<k>,<elapsed_micros>,<rows_not_skipped>` | every K pivots of Floyd-Warshall, and at least every 60 s in `fw` and `calibrate` |
| `path` | `path,<algorithm>,<source>,<target>,<distance>,<hops>,<path>,<status>` | case-study answers; path ids space-separated, source first |
| `cycle` | `cycle,<found>,<length>,<weight>,<vertices>,<status>` | the whole-graph check: `yes`/`no`, witness in arc order, its weight |
| `check` | `check,<name>,<status>,<detail>` | the check in the binary: `witness` |
| `done` | `done,<mode>` | last row of a launch |

The harness, not the binary, writes `estimate` rows into `results_part3.csv`, combining the calibration output with each step's rows.

### 9. Statuses

The memory check runs first: a step that does not fit gets `no_memory` and is not launched. `skipped` applies only to a step that would fit in memory but is turned off in the config or uses the matrix storage.

| status | emitted in | meaning |
|---|---|---|
| `ok` | stdout: `path`, `cycle`, `check`; `results_part3.csv` | computed; a repeated timing with `--runs` runs (default 10); a full all-pairs measurement; a cross-check that agreed |
| `unreachable` | stdout: `path` | no path from source to target (distance `inf`) |
| `negative_cycle` | stdout: `path` | Bellman-Ford distance −∞, or Floyd-Warshall `minus_infinity(i, j)`: a negative cycle lies on the way; no distance or path printed |
| `fewer_runs` | `results_part3.csv` | a repeated timing with 1 to `--runs` − 1 measured runs (the step budget ran out); the samples column gives the count; 1 sample has no spread |
| `extrapolated` | `results_part3.csv` (`estimate`) | not measured; computed from a model (calibration c·n³ fit, or reversal + n × mean pass time), formula in the note |
| `skipped` | `results_part3.csv` | not run by design (see above) |
| `no_memory` | `results_part3.csv` | decided before launch: (n + 1)² × 12 + graph size exceeds `MemAvailable` (from `/proc/meminfo` on Linux, `psutil.virtual_memory().available` elsewhere) minus a 1 GB margin (Floyd-Warshall on W_3 to W_5) |
| `mismatch` | `results_part3.csv` | the `bf_check` launches toward the set vertices disagree with the Floyd-Warshall `path` rows, comparing status and printed distance (harness, metric `fw_vs_bf`) |
| `out_of_range` | stdout: `path` | a requested vertex id is not in 1..n |
| `error` | stdout: `cycle`, `check`; `results_part3.csv` | a witness failed its check, or the binary exited with an error (stderr kept in the log) |

`not_implemented` from Part 2 is not used: Dijkstra is not run by the Part 3 benchmark.

### 10. Harness, config, Azure and report

* `config_part3.csv`: `graph,directed,bf_target,bf_sources,fw_set,runs,budget_seconds,k,seed,run_fw,run_allpairs,bf_optimizations`. `directed` is required; a blank value is an error, never a default.
* `analysis/harness_part3.py`:
  * **Imports** `find_input`, `plain_text`, `vertex_count`, `select_rows` and `append_csv` from `harness_weighted.py` (importing does not change Part 2).
  * **Own `check_setup`** (Part 2's needs `data/baseline.txt` and a `labels` column): the binary is built by `make benchmark-part3`, every config graph is in `data/part3/`, and `directed` is set on every row.
  * **CLI:** `harness_part3.py [--only GRAPH] [--calibrate-only]`.
  * **Inputs:** each graph is decompressed once.
  * **Memory check:** `fw_bytes_needed` follows `matrix_bytes_needed`; the need is checked before launching Floyd-Warshall (`no_memory` without launching).
  * **One file per launch:** the binary's stdout is copied line by line to `data/part3_runs/<graph>/<key>.csv`, where the key is the mode (`calibrate`, `cycle`, `bf`, `fw`, `allpairs`) or `bf_check_<t>` for a cross-check launch.
  * **Completion and restart:** a launch file is complete when its last row is `done`; only complete files are merged into `results_part3.csv` and `paths_part3.csv`. On restart, an incomplete file is renamed `<key>.csv.incomplete` and its launch is rerun; complete files are never rerun.
  * **Statistics:** mean and median from `time` rows, with the sample count and `ok` / `fewer_runs`; rows of `bf_check` launches never enter them.
  * **Estimates:** the harness writes the `estimate` rows (`status=extrapolated`).
  * **Cross-check:** after each `fw` launch, for each t of the set, `bf --target t --sources <whole set> --runs 1` (mode key `bf_check`), then the `fw_vs_bf` check comparing status and printed distance of all 20 pairs.
  * **Calibration:** `--no-bf-pass` is passed to `calibrate` when a `bf` answer run is already recorded.
  * **Units:** microseconds are converted to seconds only in the report.
* **Watchdog instead of a run-length timeout:** a launch is killed only when no row arrived for 2 × `--budget-seconds` (20 minutes by default); every mode prints rows more often than that. `fw` and `calibrate` print a `progress` row at least every 60 s. From `progress` rows the harness prints an ETA (a lower bound in the first minutes); the owner may stop a Floyd-Warshall run after about 10 minutes if the ETA exceeds the approved budget.
* `data/results_part3.csv`: `graph,mode,algorithm,metric,value,samples,status,note`. `data/paths_part3.csv`: `graph,algorithm,source,target,distance,hops,path,status`.
* `analysis/azure_part3.sh`, adapted from `azure_part2.sh` (branch `feature/part3`, `harness_part3.py`, `data/part3/` inputs, `results_part3.csv`):
  * **VM size:** 16 GB by default, such as `Standard_E2as_v5` (2 vCPUs), about 6 GB above the peak need of 7.5 GB plus the graph; 32 GB only if calibration shows memory pressure.
  * **Estimate file:** after `calibrate`, the harness writes `data/part3_runs/vm_estimate.txt` (estimated hours for the full run, one number); `start` reads it.
  * **Sequence:** owner approves `git push origin feature/part3` → `check` → `create` → `setup` (`make test`, `make benchmark-part3`, `make test-part3` on Linux) → `upload` → `calibrate` (prints the VM-time estimate; owner approves) → `start` → `status` (last `progress` row and ETA) → `fetch` → `delete`.
  * **Auto-shutdown:** a daily schedule, so `start` re-arms it to now + calibrated estimate + 3 h and refuses if that is more than 22 h ahead. If the estimate exceeds 22 h, the run is split per graph with `harness_part3.py --only <graph>`, re-arming the shutdown before each.
* `analysis/report_part3.py`: builds the four case-study tables (distances with 2 decimals, times in seconds) from the two CSVs, marking extrapolated cells and giving sample counts.

Metrics in `results_part3.csv` ("estimate rows" are those with `status=extrapolated`):

| metric | meaning |
|---|---|
| `mean_s`, `median_s` | over the measured runs; `samples` and `ok` / `fewer_runs` |
| `reverse_mean_s` | the reversal part of the Bellman-Ford time |
| `estimate_s` | `status=extrapolated`; formula in `note` |
| `memory_bytes` | the Floyd-Warshall need; `no_memory` when it does not fit |
| `negative_cycle` | `yes` / `no`; witness and weight in `note` |
| `pass_spread_s`, `targets_measured` | all-pairs Bellman-Ford |
| `rounds`, `relaxations` | per Bellman-Ford variant |
| `fw_vs_bf` | `ok` / `mismatch` |
| `max_abs_weight` | from the `info` row |

### 11. Housekeeping

* `.gitignore`: add `data/part3/`.
* Makefile: `benchmark-part3` (release, `-DNDEBUG`) and `test-part3` (builds a checked binary and runs `tests/test_harness_part3.py`); `make test` unchanged.
* The parked prototype (`stash@{0}`) is dropped once the new files exist; its round loop and pointer-cycle walk are rewritten to the pseudocode above, not copied.
* Docs: AGENTS.md (module map, programs, `edge_list` fields, Part 3 status), ONBOARDING.md §7 (Part 3 tasks), CONVENTIONS.md unchanged (the new type lives in `gl`, as storages do).

## Alternatives Considered

The author of this design is also the project owner, who decided each point below. Sources: [part3.md](part3.md) (sections "Alternatives Weighed" and "Decisions after review round 1") and the [decision log](#decision-log) above.

### Library shape

#### A. Relax over in-arcs with `for_each_in_edge` in `weighted_graph`

*   **Pros**: no reversal at run time; Bellman-Ford toward a target reads in-arcs directly.
*   **Cons**: changes `concepts.hpp` and `storage/matrix.hpp`, which AGENTS.md reserves for the project owner; every directed CSR would keep a second index of in-arcs (another m targets and m weights) whether or not Bellman-Ford ever runs.
*   **Reason for Rejection**: Rejected by author: it makes every directed graph pay for one algorithm and widens the core concept, which the reversed-graph type avoids at O(n + m) only when needed.

#### B. Reuse `shortest_paths<W>` for Bellman-Ford with `source = target`

*   **Pros**: no new result type; `reconstruct_path` already accepts it.
*   **Cons**: `reconstruct_path` follows `parent` toward the root and reverses, so on a toward-target tree it returns every path backwards (target first), and nothing fails to compile.
*   **Reason for Rejection**: Rejected by author: a silent wrong answer. `target_paths` does not satisfy `parent_tree`, so the misuse cannot compile.

#### C. Accept a plain directed CSR as "already reversed", or expose a public single-source Bellman-Ford with a thin target wrapper

*   **Pros**: no new type (first option); a familiar single-source API (second option).
*   **Cons**: with the first, passing the original graph compiles and returns distances from t labelled as distances to t; the second leaves the same mistake one call away and adds a second public result.
*   **Reason for Rejection**: Rejected by author in favour of a distinct `reversed_graph<G>` type, so the wrong graph cannot be passed and one reversal can still serve many targets.

#### D. `reversed` returning the same storage kind as its input

*   **Pros**: Bellman-Ford "on the matrix" would really run on a matrix.
*   **Cons**: a reversed 25,000-vertex matrix is another 5 GB; building it is an O(n²) scan; the assignment makes the matrix optional.
*   **Reason for Rejection**: Rejected by author: Part 3 modes run on CSR only, and matrix rows are reported as `skipped`.

### Negative cycles

#### E. Report only cycles that can reach the target

*   **Pros**: free from the single pass toward the target; the only kind that makes the requested distances undefined.
*   **Cons**: does not answer the assignment's literal question, "se o grafo possui um ciclo negativo".
*   **Reason for Rejection**: Rejected by author: the result must say whether a negative cycle exists anywhere. Bellman-Ford still reports, per vertex, which distances are undefined.

#### F. Run the whole-graph check inside every Bellman-Ford call

*   **Pros**: one call answers everything.
*   **Cons**: an O(n·m) worst-case pass repeated for every Bellman-Ford call: 10 times in case study 1 (one pass answers all three sources) and n times in all-pairs, which also makes the comparison with Floyd-Warshall (one detection) unfair.
*   **Reason for Rejection**: Rejected by author: `find_negative_cycle` is a separate graph-level function, run once per graph with its own time row, and O(1) on graphs without negative weights.

#### G. Throw on a negative cycle, as Dijkstra throws on a negative weight

*   **Pros**: one error convention for "the input breaks the algorithm".
*   **Cons**: Dijkstra's exception guards a precondition checkable in O(1) before running; a negative cycle is found only by running, and the case-study tables must show it as a value.
*   **Reason for Rejection**: Rejected by author: a cycle is found only by running, and the tables must show it as a value, so it is reported in the result.

#### H. One "cycle reaches target" flag for all sources, without marking −∞ per vertex

*   **Pros**: no extra pass.
*   **Cons**: a cycle that reaches vertex 100 from 10 does not make the distance from 20 undefined; one flag would blank all three rows.
*   **Reason for Rejection**: Rejected by author: one BFS of O(n + m) marks exactly the vertices with distance −∞, so each row is answered on its own.

#### I. A program-level rule for negative self-loops ("the whole graph is `negative_cycle`")

*   **Pros**: no io change.
*   **Cons**: cannot say which distances a self-loop makes undefined, and gives no witness; Floyd-Warshall's diagonal rule would still miss it.
*   **Reason for Rejection**: Rejected by author: `edge_list` keeps the loop edges and the algorithms take them as input.

### Algorithms

#### J. Stop Floyd-Warshall at the first negative diagonal entry

*   **Pros**: if a negative cycle appears at pivot k, the run stops after about k/n of its time.
*   **Cons**: the time no longer compares with all-pairs Bellman-Ford; distances that were still defined are thrown away; the two algorithms can disagree on the same input.
*   **Reason for Rejection**: Rejected by author: every k runs and the diagonal is checked once at the end.

#### K. One matrix per k in Floyd-Warshall

*   **Pros**: the textbook recurrence, step by step.
*   **Cons**: n + 1 matrices; at n = 25,000 that is 25,001 × 7.5 GB.
*   **Reason for Rejection**: Rejected by author: the assignment asks for the memory optimization, a single matrix updated in place.

#### L. Copy each Floyd-Warshall row into a `shortest_paths<W>` instead of a view

*   **Pros**: no change to `parent_tree`; no risk of a view outliving its matrix.
*   **Cons**: O(n) copy per row asked.
*   **Reason for Rejection**: Rejected by author in favour of a no-copy view, made safe by an lvalue-only `tree_from` and a loosened `parent_tree`.

#### M. SPFA (a FIFO queue) for the second optimization

*   **Pros**: may do fewer relaxations on sparse graphs (not measured here).
*   **Cons**: a different algorithm with a different negative-cycle rule (counting queue entries), and no notion of "round", which the first optimization and the n-th-round detection rely on.
*   **Reason for Rejection**: Rejected by author: a FIFO queue has no rounds, so optimization 1 and the round-n rule would not apply; synchronous rounds with a frontier are used instead.

#### N. Drop the optimization switches and keep only the counters

*   **Pros**: a smaller API; the tests compare against a textbook Bellman-Ford written in the test file.
*   **Cons**: the report could not compare the optimizations on real graphs.
*   **Reason for Rejection**: Rejected by author: the optimizations stay selectable as three variants (`bellman_ford_variant`); the comparison runs only on W_1, W_2 and synthetic graphs, since the textbook variant on a directed W_3 needs about 6 × 10¹¹ relaxations (n · m).

### Numbers

#### O. Plain doubles compared exactly (no scaling)

*   **Pros**: no change anywhere.
*   **Cons**: a cycle whose true weight is 0 can sum below 0 (0.3, −0.1, −0.2 gives about −2.8e−17), giving a false negative cycle and letting Bellman-Ford and Floyd-Warshall disagree.
*   **Reason for Rejection**: Rejected by author for the program, in favour of fixed-point weights × 100 parsed from the text. The library itself keeps this behaviour for users who do not scale.

#### P. A tolerance ε on every comparison

*   **Pros**: works for any input precision.
*   **Cons**: ε is arbitrary; a real negative cycle lighter than ε is missed; the relaxation rule changes in both algorithms and every test must state its ε.
*   **Reason for Rejection**: Rejected by author: the inputs have at most 2 decimals, so exact fixed point removes the problem instead of hiding it.

#### Q. Scaling by `round(w * 100)` on the parsed double

*   **Pros**: works with the existing reader as is.
*   **Cons**: rounding comes back in at the conversion, and a 3-decimal weight would be rounded silently instead of rejected.
*   **Reason for Rejection**: Rejected by author: rounding would return and 3-decimal input would be accepted silently, so the weight is parsed from its text.

#### R. Parsing weights as text in a separate reader in the program, or in a second pass over the file

*   **Pros**: no change to the io contract.
*   **Cons**: the separate reader duplicates about 80 lines of checks that can drift and cannot be reached from `tests/`; the second pass reads the file twice and pairs weights with the wrong edges if its skipping rules differ.
*   **Reason for Rejection**: Rejected by author in favour of a parser hook in `read_edge_list`.

### Running the case studies

#### S. Repeat every algorithm 10 times regardless of its length

*   **Pros**: a measured mean everywhere.
*   **Cons**: 10 Floyd-Warshall runs at n = 25,000 are estimated at 50 to 100 VM-hours per graph; all-pairs Bellman-Ford would take months on W_4 and years on W_5.
*   **Reason for Rejection**: Rejected by author: repeated runs share a time budget per step (600 s); a step whose first run exceeds it reports that single measured run (`fewer_runs`, 1 sample).

#### T. A fixed 20 s limit per run

*   **Pros**: simple to state and to check.
*   **Cons**: the Bellman-Ford pass on W_4 (estimated 5 to 20 s) and W_5 (estimated 20 to 100 s) would sit at or over the limit, so their "mean" would be one sample; raising it costs little VM time compared with the Floyd-Warshall runs.
*   **Reason for Rejection**: Rejected by author in favour of a 600 s budget per step, which gives measured means on W_4 and W_5 and still caps VM time.

#### U. Skip any run over the time limit, including the answer runs

*   **Pros**: the least VM time.
*   **Cons**: case study 2 would have no Floyd-Warshall distances on any graph.
*   **Reason for Rejection**: Rejected by author: the time budget applies to repeated timing only; answer runs run once.

#### V. Answer case study 2 with Bellman-Ford toward each of the five vertices

*   **Pros**: real distances at low cost.
*   **Cons**: the assignment says "Utilizando o algoritmo de Floyd-Warshall".
*   **Reason for Rejection**: Rejected by author: Floyd-Warshall answers case study 2; Bellman-Ford toward those vertices is used only as a cross-check.

#### W. Run on the owner's Mac

*   **Pros**: no cloud cost.
*   **Cons**: hours-long Floyd-Warshall runs on a 16 GiB laptop, close to its memory limit, where swapping would spoil the timing.
*   **Reason for Rejection**: Rejected by author: one Azure VM (student plan), the smallest size that fits, deleted after the run.

#### X. A separate k-block calibration run on the real graph

*   **Pros**: measures Floyd-Warshall's cost per pivot on the real W_1 and W_2 (their reachability and memory traffic), unlike the random-graph fit.
*   **Cons**: a second, partial run on each graph before the answer run.
*   **Reason for Rejection**: Rejected by author: the answer run's own `progress` rows give the same measurement within its first minutes, at no extra cost; the random-graph fit is kept only for the pre-run VM estimate and for the graphs that are not run.

#### Y. Tiled (blocked) in-place Floyd-Warshall

*   **Pros**: about B times less memory traffic per pivot for B × B tiles, with the same single-matrix memory, so a shorter run when the plain loop is bandwidth-bound.
*   **Cons**: a different loop structure from the class algorithm; harder to combine with the row skip and the predecessor matrix; more code to test.
*   **Reason for Rejection**: Rejected by author for this project: the case study asks for the class algorithm with its memory optimization; the progress ETA guards against a slow run, and the owner may revisit it if calibration shows the plain loop is far over budget.

#### Z. Two Floyd-Warshall kernels raced on the VM, plus a bandwidth model

*   **Pros**: picks the faster inner loop on the real hardware, and gives a second, bandwidth-based estimate.
*   **Cons**: two kernels, a reference loop and extra row kinds, for at most a constant factor; the bandwidth figure fed only an estimate the live ETA already replaces.
*   **Reason for Rejection**: Rejected by author: one kernel (§7), and the answer run's progress rows guard the VM budget.

--------------------------------------------------------------------------------

## Risks & Mitigations

| risk | trigger | impact | mitigation | handled in | decides |
|---|---|---|---|---|---|
| A Floyd-Warshall answer run dies or the VM is deallocated mid-run | auto-shutdown, credit, crash | hours of VM time lost | progress rows and a watchdog; auto-shutdown re-armed to now + estimate + 3 h before `start`; one graph per session if the estimate exceeds 22 h; the cross-check runs in separate launches, so a crash loses at most the Floyd-Warshall launch itself | §8, §10 | owner (VM budget) |
| The calibration fit is wrong at n = 25,000 (cache, reachability); the run is bandwidth-bound, estimated 5 to 10 h | random graphs do not match W_1, W_2 | VM budget approved on a wrong number | the answer run's progress rows give a live ETA after about 10 minutes; the owner may stop it | §8, §10, Alternative X | owner |
| Directed W_1 has a negative cycle | unknown until it runs | case studies 1 and 2 partly −∞ | per-vertex (Bellman-Ford) and per-pair (Floyd-Warshall) marking; other answers stay exact | §4, §7 | — |
| A false negative cycle from rounding | 2-decimal weights in doubles | wrong answers, disagreeing algorithms | fixed-point weights; path-sum bound stated | §1 | — |
| A path walk loops on a corrupt or cyclic tree | a caller reconstructs a path for a pair with `minus_infinity(i, j)` or a −∞ vertex, and release builds do not check this | a hung benchmark | the program asks for paths only for `ok` pairs; `path_to_target` returns early; `reconstruct_path` is bounded | §4, §6, §7 | — |
| The reversal's temporary copy on W_5 | m = 40 M arcs | about 640 MB extra while building | fits the 16 GB VM; stated | §3 | — |
| The Azure run needs `feature/part3` on GitHub | `setup` clones the branch | a push without approval violates the AGENTS.md rule that pushes need the owner's approval | the owner approves the push as the first step of the Azure sequence | §10 | owner |
| Student credit runs out | long runs | the run stops | VM-time estimate approved before `start`; VM deleted after `fetch` | §10 | owner |

--------------------------------------------------------------------------------

## Non-Functional Requirements

*   **Performance & memory:** Floyd-Warshall needs (n + 1)² × 12 bytes; the harness decides `no_memory` before launch. No timed region includes disk I/O. Repeated timing within a 600 s budget per step.
*   **Cost:** one Azure VM on a student plan, smallest size that fits (16 GB), deleted after `fetch`; auto-shutdown re-armed before `start` (§10).
*   **Security & privacy:** no credentials are written to the repository; the Azure script uses the owner's existing `az login`.
*   **Rollback:** each Implementation Plan item is its own commit on `feature/part3`, nothing is pushed without the owner's approval, and Part 1 / Part 2 programs are untouched.

--------------------------------------------------------------------------------

## Detailed Implementation Plan

Work happens on `feature/part3`. Each numbered commit below is atomic, uses Conventional Commits with no mention of any assistant, and leaves `make test` passing on macOS (hardening + UBSan) with no new warnings in `make` or, from commit 11 on, `make benchmark-part3`. Expected values come from hand computation (written in the test as a comment) or from an independent reference named in the test. Tests on integer-valued (or scaled 2-decimal) weights compare distances exactly, because those sums are exact (see the fixed-point decision in the Decision log); comparisons with Dijkstra on non-integer random weights use `doctest::Approx`. Never committed: `design_draft.md` at the repository root, `presentation/parte2/*`, `data/part3/`, `data/part3_runs/`, `build/`.

### Commit order

| # | commit message | files |
|---|---|---|
| 1 | `docs: Part 3 design, review reports; ignore Part 3 inputs` | `.gitignore`, `AGENTS.md` (Git section, already edited), `docs/designs/part3.md`, `docs/designs/part3_design.md`, `docs/designs/part3_review/*.md` |
| 2 | `refactor(io): keep negative self-loop edges instead of counting them` | `include/graph/io/edge_list.hpp`, `tests/test_edge_list.cpp` |
| 3 | `feat(io): weight parser hook in read_edge_list` | `include/graph/io/edge_list.hpp`, `tests/test_edge_list.cpp` |
| 4 | `feat(io): fixed_point_weight parser` | `include/graph/io/fixed_point.hpp`, `tests/test_fixed_point.cpp`, `tests/data/three_decimals.txt` |
| 5 | `feat(storage): reversed_graph and reversed()` | `include/graph/storage/reversed.hpp`, `tests/test_reversed.cpp` |
| 6 | `feat(algorithms): parent_tree accepts spans; bounded reconstruct_path` | `include/graph/algorithms/path.hpp`, `tests/test_path.cpp` |
| 7 | `feat(algorithms): Bellman-Ford toward a target with three variants` | `include/graph/algorithms/bellman_ford.hpp`, `tests/test_bellman_ford.cpp` |
| 8 | `feat(algorithms): whole-graph negative cycle check` | `include/graph/algorithms/bellman_ford.hpp`, `tests/test_bellman_ford.cpp` |
| 9 | `test(algorithms): Bellman-Ford against independent references` | `tests/test_bellman_ford_differential.cpp` |
| 10 | `feat(algorithms): Floyd-Warshall with one in-place matrix` | `include/graph/algorithms/floyd_warshall.hpp`, `tests/test_floyd_warshall.cpp`, `tests/test_floyd_warshall_differential.cpp`, `tests/data/zero_cycle_decimals.txt` |
| 11 | `feat(app): Part 3 benchmark` | `app/benchmark_part3.cpp`, `Makefile` (`benchmark-part3`, `build/benchmark_part3_checked`) |
| 12 | `feat(analysis): Part 3 harness and config` | `analysis/harness_part3.py`, `config_part3.csv`, `tests/test_harness_part3.py`, `tests/data/part3_small.txt`, `tests/data/part3_cycle.txt`, `Makefile` (`test-part3`, `.PHONY`) |
| 13 | `feat(analysis): Azure script and report tables for Part 3` | `analysis/azure_part3.sh`, `analysis/report_part3.py` |
| 14 | `docs: Part 3 module map, programs and tasks` | `AGENTS.md`, `docs/ONBOARDING.md`, `README.md` (only where wrong) |
| 15 | `data: Part 3 case-study results` (after the owner-approved Azure run) | `data/azure_part3/*`, `docs/results/part3_tables.md` |

After commit 8, and with the owner's approval, `git stash drop` removes the parked prototype.

### Target Files to Create/Modify

#### 1. `.gitignore` (Modify), commit 1

*   **Type / Scope**: repository configuration.
*   **Proposed Changes**: add `data/part3/` and `data/part3_runs/`.

#### 2. `AGENTS.md` (Modify), commits 1 and 14

*   **Type / Scope**: agent instructions.
*   **Proposed Changes**: commit 1 carries the Git-section edit already made. Commit 14: module map entries for `io/fixed_point.hpp`, the `read_edge_list` parser overload and the `negative_self_loop_edges` field, `storage/reversed.hpp` (`reversed_graph`, `is_reversed_graph`, `reversed`), `algorithms/bellman_ford.hpp` (`target_paths`, `bellman_ford_variant`, `bellman_ford_options`, `bellman_ford_stats`, `bellman_ford`, `bellman_ford_on_reversed`, `path_to_target`, `negative_cycle_report`, `find_negative_cycle`), `algorithms/floyd_warshall.hpp` (`all_pairs`, `floyd_warshall`, `no_op_pivot`), the loosened `parent_tree`; the Part 3 programs (`benchmark_part3`, `harness_part3.py`, `config_part3.csv`, `azure_part3.sh`, `report_part3.py`, `make benchmark-part3`, `make test-part3`); the project status line.

#### 3. `docs/designs/part3.md`, `docs/designs/part3_design.md`, `docs/designs/part3_review/*.md` (Add), commit 1

*   **Type / Scope**: design documents.
*   **Proposed Changes**: the brief (its status line already points to the design), this design with its final status line, and the review reports the Decision log links to.

#### 4. `include/graph/io/edge_list.hpp` (Modify), commits 2 and 3

*   **Type / Scope**: C++ header, `gl::io`.
*   **Proposed Changes**:
    *   Commit 2: `std::size_t negative_self_loops` becomes `std::vector<input_edge_t<W>> negative_self_loop_edges` (the `v v w` lines with w < 0, file order; filled only when `W::is_weighted`); `skipped_self_loops` unchanged; comment updated.
    *   Commit 3: the body moves into `detail::read_edge_list_impl<W>(path, parse_weight)`. The weight field is read as the next whitespace-separated token, only under `if constexpr (W::is_weighted)`; a missing token gives "expected an edge weight"; an `std::invalid_argument` from `parse_weight` becomes `detail::parse_error(path, line_no, e.what())`; trailing-text detection stays after the weight. The public overload `read_edge_list<W>(path, parse_weight)` (constraint as in §1) calls it. The existing `read_edge_list<W>(path)` passes `detail::stream_weight<typename W::weight_type>`, which extracts the token with `std::istringstream`, throws "expected an edge weight" when extraction fails and "unexpected text after the edge" when the token is not fully consumed, so today's messages for `1 2 x` and `1 2 0.5x` are kept.

#### 5. `include/graph/io/fixed_point.hpp` (Create), commit 4

*   **Type / Scope**: C++ header, `gl::io`.
*   **Proposed Changes**: `fixed_point_weight<W>` as in §1. Parses sign, integer digits, optional point and 1..`decimals` fractional digits, padding the fraction to `decimals` digits. Accumulates in `long long acc`; before each digit step checks `acc > (limit - digit) / 10`, where `limit = 2^std::numeric_limits<W>::digits`, and throws "weight too large to hold exactly: <token>" instead of overflowing. Other messages: "weight has more than N decimals: <token>", "not a decimal weight: <token>". `scale()` returns 10^decimals. Assert `decimals <= 9`.

#### 6. `include/graph/storage/reversed.hpp` (Create), commit 5

*   **Type / Scope**: C++ header, `gl`.
*   **Proposed Changes**: as in §3, in this order: forward declaration of `reversed_graph`; `is_reversed_graph<G>` (false; true for `reversed_graph<H>` specializations); the class (private CSR member and private constructor; `direction_type`, `weighting_type`, `weight_type`; `vertex_count`, `for_each_neighbor`, `for_each_edge`, `has_negative_weight` forwarding to the CSR; the constrained `friend reversed`); then `reversed(g)`, which collects the swapped arcs with `for_each_edge` and calls `csr_graph<directed, …>::from_edges`.

#### 7. `include/graph/algorithms/path.hpp` (Modify), commit 6

*   **Type / Scope**: C++ header, `gl::algorithms` (owner-approved contract change).
*   **Proposed Changes**: `parent_tree` as in §6; `reconstruct_path` stops after `tree.parent.size()` steps and returns `{}`; comments state both.

#### 8. `include/graph/algorithms/bellman_ford.hpp` (Create), commits 7 and 8

*   **Type / Scope**: C++ header, `gl::algorithms`.
*   **Proposed Changes**:
    *   Commit 7: `target_paths`, `bellman_ford_variant`, `bellman_ford_options`, `bellman_ford_stats`, the `reverse_input` concept, `detail::relax_rounds`, `detail::mark_minus_infinity`, `bellman_ford_on_reversed`, `bellman_ford`, `path_to_target`. Signatures: `template<weighted_graph R> std::vector<vertex_id> detail::relax_rounds(const R& rev, std::vector<W>& dist, std::vector<vertex_id>& next, std::vector<vertex_id> initial_frontier, bellman_ford_variant variant, bellman_ford_stats& stats)` returns the vertices changed in round n (empty if the loop stopped earlier), following the synchronous pseudocode of §4 (`prev` array, copy of changed entries after each round); `detail::mark_minus_infinity(const R& rev, std::span<const vertex_id> seeds, target_paths<W>&)` is the BFS of §4. Assert messages: `bellman_ford: target out of range`, `path_to_target: vertex out of range`.
    *   Commit 8: `negative_cycle_report`, `detail::pointer_cycle(const std::vector<vertex_id>& pred, vertex_id start)` (walk n steps, collect the cycle, return it in arc order), `find_negative_cycle` (O(1) and O(m) shortcuts of §5, then `relax_rounds` with all-zero `dist` and every vertex in the first frontier on g itself, frontier variant).

#### 9. `include/graph/algorithms/floyd_warshall.hpp` (Create), commit 10

*   **Type / Scope**: C++ header, `gl::algorithms`.
*   **Proposed Changes**: `no_op_pivot`, `all_pairs<W>` (members as in §7: `vertex_count`, `dist`, `minus_infinity`, `negative_cycle`, `tree_from` lvalue-only with the rvalue overload deleted, `bytes`; private `dist_`, `pred_`, `on_cycle_`) and `floyd_warshall(g, negative_loops, on_pivot)`, following the §7 algorithm: lightest parallel arc at initialisation, self-loop seeds on the diagonal, the one kernel of the implementation note (row i = k skipped, rows with dist(i, k) = +inf skipped, cell written only when improved), `on_pivot(k, rows_not_skipped)` after each k, `pred(i, i) = 0` reset, `on_cycle_` filled at the end. Cell index computed in `std::size_t`.

#### 10. `app/benchmark_part3.cpp` (Create), commit 11

*   **Type / Scope**: C++ program on the new library only.
*   **Proposed Changes**: command line, modes, budget rule and output rows as in §8. Specifics:
    *   `--directed` missing or not `yes|no` is an error. Vertex ids given on the command line are checked against 1..n before any library call; a bad one gives a `path` row with status `out_of_range`.
    *   Loading: `read_edge_list<gl::weighted<double>>(path, io::fixed_point_weight<double>{weight_decimals})`; right after loading, `info,max_abs_weight` is printed and the program exits with an error if (n − 1) × max |scaled w| > 2^53.
    *   Undirected graphs: `bf` and `allpairs` call `bellman_ford_on_reversed(g, t)` directly and print no `reverse` row.
    *   `allpairs` order: `std::shuffle` of 1..n with `std::mt19937_64(seed)`; when the step stops early, the targets used are the first ones of that order.
    *   `calibrate`: random directed graphs with integer scaled weights 0..1000 and the input's average out-degree, sizes 500, 1000, 2000, …; `fit,floyd_warshall_n3,<c>,<points>` is the least-squares fit through the origin of time = c · n³ over the `calib` points; a `progress` row after each run.
    *   `fw`: the `on_pivot` observer prints a `progress` row when 250 pivots or 60 s have passed since the last one.
    *   `cycle`: the witness check of §8 (`check,witness,ok|error`, `cycle` row with weight).
    *   Distances printed as `std::format("{:.2f}", d / scale)`; `inf` and `-inf` as such.
    *   Errors: `error: <message>` on stderr and exit status 1, as in `benchmark_weighted`.
    *   Helpers copied from `benchmark_weighted.cpp` (`parse_integer`, `csv_field`), not shared.

#### 11. `Makefile` (Modify), commits 11 and 12

*   **Type / Scope**: build.
*   **Proposed Changes**: commit 11: `benchmark-part3` → `build/benchmark_part3` with `RELFLAGS -DNDEBUG`, and `build/benchmark_part3_checked` with `TESTFLAGS`. Commit 12: `test-part3` builds the checked binary and runs `python3 -m unittest discover -s tests -p 'test_harness_part3.py'` with `PART3_BINARY` set to it; `.PHONY` updated. `make test` unchanged.

#### 12. `config_part3.csv` (Create), commit 12

*   **Type / Scope**: harness input.
*   **Proposed Changes**: one row per `grafo_W_1` … `grafo_W_5`: `directed=yes`, `bf_target=100`, `bf_sources=10|20|30`, `fw_set=10|20|30|40|50`, `runs=10`, `budget_seconds=600`, `k=100`, `seed=2025`, `run_fw=yes` for W_1 and W_2 and `no` for W_3 to W_5 (the memory check gives `no_memory` first anyway), `run_allpairs=yes`, `bf_optimizations=textbook|stop_when_stable|frontier` for W_1 and W_2 and empty for W_3 to W_5 (the textbook variant needs about n·m = 6 × 10¹¹ relaxations on W_3; Alternative N). The harness turns `|` into `,` for `--sources`, `--set` and `--optimizations`.

#### 13. `analysis/harness_part3.py` (Create), commit 12

*   **Type / Scope**: Python 3 + pandas + psutil.
*   **Proposed Changes**, as in §10:
    *   Import: inserts its own folder into `sys.path`, then `from harness_weighted import find_input, plain_text, vertex_count, select_rows, append_csv`.
    *   `read_config(path)` (rejects a blank `directed`), `check_setup(binary, data, rows)` (binary from `make benchmark-part3`, every graph in `data`, `directed` set).
    *   `fw_bytes_needed(graph)` = (n + 1)² × 12 + the file size; `decide_fw(graph, available)` → `run` or `no_memory`, with `available` = `psutil.virtual_memory().available` − 1 GiB.
    *   `launch(graph, key, args, out_file, silence_limit)`: runs the binary, copies stdout line by line to `out_file`, kills it if no line arrives for `silence_limit` seconds, returns whether the file ends with `done`.
    *   `parse_row(line)` for the nine row kinds (`info`, `time`, `calib`, `fit`, `progress`, `path`, `cycle`, `check`, `done`), raising on anything else.
    *   `summarize(time_rows, runs)` → mean, median, samples, `ok` / `fewer_runs`; `estimate_rows(...)` for the `extrapolated` metrics; `eta(progress_rows, n)`.
    *   `merge(run_dir)` builds `data/results_part3.csv` and `data/paths_part3.csv` from complete files only; `bf_check_*` files feed only the `fw_vs_bf` metric.
    *   Per graph, launch order: `calibrate` (with `--no-bf-pass` if a complete `bf` file exists), `cycle`, `bf` (with `--optimizations` from the config), `fw` (if `decide_fw` says run), one `bf_check_<t>` per t of the set (`--runs 1 --sources <set>`), `allpairs`; matrix rows written as `skipped`. Complete files are never rerun; an incomplete file is renamed `.incomplete` and relaunched.
    *   CLI: `harness_part3.py [--data data/part3] [--only GRAPH] [--calibrate-only]`; after `--calibrate-only` it writes `data/part3_runs/vm_estimate.txt`.

#### 14. `analysis/azure_part3.sh` (Create), commit 13

*   **Type / Scope**: Bash, Azure CLI.
*   **Proposed Changes**: adapted from `azure_part2.sh`: `AZ_GROUP=cos242-part3`, `BRANCH=feature/part3`, `AZ_SIZES="Standard_E2as_v5 Standard_E2s_v5"`; `upload` copies `data/part3/grafo_W_*.txt.gz` to `$REMOTE_DIR/data/part3/`; `setup` runs `make test`, `make benchmark-part3` and `make test-part3`; `calibrate` runs `harness_part3.py --calibrate-only` and prints `vm_estimate.txt`; `start` reads it, re-arms the auto-shutdown to now + estimate + 3 h, refuses if that is more than 22 h ahead (then it prints the per-graph `--only` commands to run one at a time), and starts the harness in tmux; `status` prints the last `progress` row and the ETA, in the style of `analysis/azure_watchdog.py`; `fetch` copies the CSVs and run files into `data/azure_part3/`.

#### 15. `analysis/report_part3.py` (Create), commit 13

*   **Type / Scope**: Python 3.
*   **Proposed Changes**: reads `data/azure_part3/results_part3.csv` and `paths_part3.csv`; writes `docs/results/part3_tables.md` with the four case-study tables (distances with 2 decimals; times in seconds, 3 significant digits, with the sample count; extrapolated cells marked with a footnote giving the formula; `no_memory` cells with the memory need).

#### 16. `tests/test_edge_list.cpp` (Modify), commits 2 and 3

*   **Type / Scope**: doctest.
*   **Proposed Changes**: commit 2: the negative-self-loop test checks the vector (`{1, 1, -2}`, `{2, 2, -0.5}` in order; zero and positive loops not kept; unweighted stays empty). Commit 3: a helper `read_string_with(content, parser)` next to `read_string`; on the input `"3\n1 2 0.5\n2 3 -1.5\n"` a recording parser receives exactly `"0.5"` and `"-1.5"`; a parser throwing `std::invalid_argument("bad")` on line 3 gives a `runtime_error` containing `:3: bad`; a missing weight with a custom parser still says "expected an edge weight"; the default reader rejects `"3\n1 2 0.5x\n"` with "unexpected text after the edge"; a local concept shows the overload rejects an unweighted policy and a parser returning `std::string`.

#### 17. `tests/test_fixed_point.cpp`, `tests/data/three_decimals.txt` (Create), commit 4

*   **Type / Scope**: doctest + fixture (`2\n1 2 1.234\n`).
*   **Proposed Changes**: with `decimals = 2`: `"1.19"` → 119, `"-5.00"` → −500, `"10"` → 1000, `"0.3"` → 30, `"-0.1"` → −10, `"+2.5"` → 250, `"0"` → 0; rejected `"1.234"`, `"1e2"`, `"."`, `"-"`, `"1."`, `"abc"`, `"1.2x"`, a 25-digit token. With `decimals = 0`: `"7"` → 7, `"7.5"` rejected, `"9007199254740992"` accepted and `"9007199254740993"` rejected for `double`, `"16777217"` rejected for `float`. `scale()` is 100 for 2 decimals. `static_assert` that `fixed_point_weight<int>` is rejected. End to end: `read_edge_list<gl::weighted<double>>("tests/data/three_decimals.txt", fixed_point_weight<double>{2})` throws with `three_decimals.txt:2:` and `more than 2 decimals`.

#### 18. `tests/test_reversed.cpp` (Create), commit 5

*   **Type / Scope**: doctest.
*   **Proposed Changes**: on directed CSR and directed matrix inputs with arcs 1→2 (3), 2→3 (4), 1→3 (5), 3→1 (−2) and vertex 4 isolated, `for_each_edge` on the reversal lists 1: {(3, −2)}, 2: {(1, 3)}, 3: {(1, 5), (2, 4)}, 4: {}; `has_negative_weight()` is true; parallel CSR arcs are kept (compared sorted); `static_assert` that the reversal is a `weighted_graph` with `direction_type = directed`, that `is_reversed_graph` is true for it and false for `csr_graph`; local concepts show `reversed` rejects an undirected graph, an unweighted directed graph, and a `reversed_graph` (`reversed(reversed(g))`).

#### 19. `tests/test_path.cpp` (Modify), commit 6

*   **Type / Scope**: doctest.
*   **Proposed Changes**: `static_assert(parent_tree<search_tree>)` and for `shortest_paths<double>`; a struct with `source` and a `std::span<const vertex_id> parent` satisfies it and reconstructs `1 2 3` from parents {0, 0, 1, 2}; negative `static_assert`s for `std::list<vertex_id>`, `std::vector<int>` and a type without `source`; a corrupt tree with parent[2] = 3, parent[3] = 2 and source 1 returns `{}` for target 2.

#### 20. `tests/test_bellman_ford.cpp` (Create), commits 7 and 8

*   **Type / Scope**: doctest; directed cases use `TEST_CASE_TEMPLATE` over directed CSR and directed matrix; undirected cases are separate.
*   **Proposed Changes**, all values by hand:
    *   Commit 7:
        *   Negative arcs, no cycle: arcs 1→2 (4), 1→3 (2), 3→2 (−1), 2→4 (3), 3→4 (6), 4→5 (1), target 4: dist = {·, 4, 3, 2, 0, +inf}, next = {·, 3, 4, 2, 0, 0}; `path_to_target(1)` = 1 3 2 4, `(5)` = {}, `(4)` = {4}.
        *   Single vertex; unreachable (1→2 (1), 3→4 (1), target 2: dist {·, 1, 0, inf, inf}).
        *   Undirected, n = 4 with vertex 4 isolated: 1-2 (10), 1-3 (1), 3-2 (1), target 2: dist {·, 2, 0, 1, inf}, next {·, 3, 0, 2, 0}.
        *   Cycle reaching the target, partly: 1→2 (1), 2→3 (−3), 3→2 (1), 3→4 (1), 5→4 (2), target 4: dist[1..3] = −inf, dist[5] = 2, `cycle_reaches_target`; `path_to_target(1)` = {}.
        *   Cycle not reaching the target: 1→2 (1), 2→1 (−3), 3→4 (2), 4→1 (1), target 4: dist {·, inf, inf, 2, 0}, not `cycle_reaches_target`.
        *   Ring 1→2→…→6→1 with weights 1, 1, 1, 1, 1, −6, target 1: every dist −inf (detection exactly in round n).
        *   A target on a negative cycle: 1→2 (−2), 2→1 (1), target 1: `path_to_target(1)` = {}.
        *   Zero-weight cycle: 1→2 (0), 2→1 (0), 2→3 (1), target 3: dist {·, 1, 1, 0}.
        *   Self-loop as the only negative cycle (regression for §4 step 1): arcs 1→2 (1), 2→3 (1), loop (2, −1) passed in, target 3: the rounds stop early, yet dist[1], dist[2] = −inf and `cycle_reaches_target`; without the loop list the same graph gives dist {·, 2, 1, 0}. Shown to fail with the self-loop seeding removed.
        *   A loop on a vertex that cannot reach the target (loop (4, −1), 4 isolated): not `cycle_reaches_target`.
        *   Pruning, star of 99 arcs i→100 (1), target 100: textbook 100 rounds / 9,900 relaxations; stop_when_stable 2 / 198; frontier 2 / 99; same dist.
        *   `bellman_ford(g, t)` equals `bellman_ford_on_reversed(reversed(g), t)` field by field.
        *   A hand-made corrupt `target_paths` whose `next` has a cycle: `path_to_target` returns {} after n steps.
        *   Compile-time: `bellman_ford` rejects an unweighted graph and a `reversed_graph`; `bellman_ford_on_reversed` rejects a plain directed CSR; `target_paths` is not a `parent_tree`; `target_paths<int>` is rejected.
        *   `float` weights: one small graph, `dist` has element type `float`.
    *   Commit 8 (`find_negative_cycle`; witnesses compared up to rotation, in arc order, v0 not repeated):
        *   The graph of "cycle reaching the target, partly": witness {2, 3} with arc weights summing to −2.
        *   The graph of "cycle not reaching the target": witness {1, 2}.
        *   Loop (4, −1) passed in: found, witness {4}, stats.rounds = 0.
        *   Undirected 1-2 (−1): found, witness {1, 2}, stats.rounds = 0.
        *   A non-negative graph: not found, stats.rounds = 0.
        *   Ring of 6 with total weight −1: witness is the whole ring.

#### 21. `tests/test_bellman_ford_differential.cpp` (Create), commit 9

*   **Type / Scope**: doctest, fixed seeds (`std::mt19937`), shapes from tiny to n = 200.
*   **Proposed Changes**:
    *   Non-negative random digraphs: for every source s and target t, `bellman_ford(g, t).dist[s]` equals `dijkstra_heap(g, s).dist[t]` (Approx; inf equal).
    *   Potentials: integer w in 0..9, integer p in 0..19, w' = w + p(u) − p(v); expected dist(u → t) = d_w(u, t) + p(u) − p(t), with d_w from `dijkstra_heap` on a reversed edge list the test builds itself; exact; no cycle reported.
    *   Planted cycle: arcs a→b (2), b→c (2), c→a (−5) added to a random non-negative digraph: `find_negative_cycle` finds a witness whose arcs exist and whose weight is negative; `cycle_reaches_target` equals "a reaches t" by `bfs`; dist[v] is −inf exactly for the v that reach a, when a reaches t.
    *   Brute force for n ≤ 6, weights in −3..5: simple cycles and simple paths enumerated; both functions compared.
    *   Variants: on every random graph, the three variants give the same dist, stop_when_stable and frontier the same rounds, and textbook ≥ stop_when_stable ≥ frontier in both `rounds` and `relaxations`.
    *   Every finite `next` chain is a shortest-path tree: dist[v] = w(v, next[v]) + dist[next[v]] with the lightest parallel arc.

#### 22. `tests/test_floyd_warshall.cpp`, `tests/data/zero_cycle_decimals.txt` (Create), commit 10

*   **Type / Scope**: doctest + fixture (`3\n1 2 0.3\n2 3 -0.1\n3 1 -0.2\n`).
*   **Proposed Changes**:
    *   Hand graph: arcs 1→2 (3), 2→3 (−2), 1→3 (4), 3→1 (1), vertex 4 isolated: rows 1: {0, 3, 1, inf}, 2: {−1, 0, −2, inf}, 3: {1, 4, 0, inf}, 4: {inf, inf, inf, 0}; `reconstruct_path(tree_from(3), 2)` = 3 1 2; `vertex_count()` = 4; `bytes()` = 5² × 12.
    *   `minus_infinity` on the "cycle reaching the target, partly" graph: (1, 4), (2, 4), (3, 4) true; (5, 4) false with dist 2; `negative_cycle()` true; `pred(i, i)` = 0 for every i; `reconstruct_path(tree_from(5), 4)` = 5 4.
    *   Negative self-loop (3, −1) passed in: `negative_cycle()`, `minus_infinity(3, 3)`.
    *   Parallel CSR arcs 1→2 (5), 1→2 (2): dist(1, 2) = 2.
    *   `on_pivot`: a counting observer is called n times; on the hand graph, `rows_not_skipped` at k = 4 is 0.
    *   `static_assert(parent_tree<all_pairs<double>::row_tree>)`; `all_pairs<int>` rejected; `tree_from` on an rvalue does not compile (local concept).
    *   Scaled regression from the fixture: with `read_edge_list<gl::weighted<double>>(…, fixed_point_weight<double>{2})`, Floyd-Warshall and `find_negative_cycle` report no negative cycle; `bellman_ford(g, 1).dist[2]` = −30 exactly and equals `dist(2, 1)`. Control: the test first checks `(0.3 + -0.1) + -0.2 < 0`, then that unscaled Floyd-Warshall reports the false cycle.

#### 23. `tests/test_floyd_warshall_differential.cpp` (Create), commit 10

*   **Type / Scope**: doctest, fixed seeds, n up to 60.
*   **Proposed Changes**: on potential-built graphs every pair equals `bellman_ford(g, j).dist[i]` exactly; on non-negative graphs every row equals `dijkstra_heap(g, i).dist` (Approx); for every row and reachable j, `reconstruct_path(tree_from(i), j)` starts at i, ends at j, uses existing arcs, and its lightest-arc weights sum to dist(i, j); on planted-cycle graphs `minus_infinity(i, j) == (bellman_ford(g, j).dist[i] == -inf)` for every pair, the other cells are equal, and `negative_cycle()` equals `find_negative_cycle(g).found`.

#### 24. `tests/data/part3_small.txt`, `tests/data/part3_cycle.txt`, `tests/test_harness_part3.py` (Create), commit 12

*   **Type / Scope**: fixtures + Python unittest (run by `make test-part3`).
*   **Proposed Changes**:
    *   `part3_small.txt`: n = 120; arcs 10→100 (1.25), 20→10 (0.50), 40→20 (−0.25), 50→40 (2.00), 100→50 (1.00); every other vertex isolated. Expected `bf --target 100 --sources 10,20,30`: 10 → 1.25 (10 100), 20 → 1.75 (20 10 100), 30 `unreachable`. Expected `fw --set 10,20,30,40,50`: from 10: 20 → 4.00, 40 → 4.25, 50 → 2.25; from 20: 10 → 0.50, 40 → 4.75, 50 → 2.75; from 40: 10 → 0.25, 20 → −0.25, 50 → 2.50; from 50: 10 → 2.25, 20 → 1.75, 40 → 2.00; every pair with 30 `unreachable`. No negative cycle (the only cycle weighs 4.50).
    *   `part3_cycle.txt`: n = 5; arcs 1→2 (1.00), 2→3 (−3.00), 3→2 (1.00), 3→4 (1.00), 5→4 (2.00). Expected: `cycle` finds {2, 3} with weight −2.00 and `check,witness,ok`; `bf --target 4 --sources 1,5`: 1 `negative_cycle`, 5 → 2.00; `fw --set 1,4,5`: (1, 4) `negative_cycle`, (5, 4) 2.00, (4, 1) `unreachable`.
    *   Binary end to end (checked binary): the rows above; `--budget-seconds 0 --runs 10` prints exactly one `time` row per step; `progress` and `done` rows present; `--directed` missing → non-zero exit; a source id 999 → `out_of_range`; `three_decimals.txt` → non-zero exit with `three_decimals.txt:2:`; `allpairs` on `part3_small.txt` with `--budget-seconds 600` measures all 120 targets (`ok`), and with `--k 5 --budget-seconds 0` stops after one pass.
    *   Harness units: `parse_row` for the nine kinds and a malformed line; `summarize` gives `ok` for 10 `time` rows and `fewer_runs` with 6; `read_config` rejects a blank `directed`; `decide_fw` gives `no_memory` for n = 100,000 and `run` for n = 25,000 at 16 GiB available; `merge` ignores a file without `done` and the `bf_check` rows in the statistics; `fw_vs_bf` gives `mismatch` when a `bf_check` row is edited; restart renames an incomplete file and relaunches only it; the watchdog kills a fake binary (a Python script that prints one row and sleeps) after the silence limit; matrix rows are `skipped`.

#### 25. `docs/ONBOARDING.md` (Modify), commit 14

*   **Type / Scope**: Portuguese onboarding guide.
*   **Proposed Changes**: §7 gets Part 3 tasks (I: io hook and fixed point; J: reversed graph; K: Bellman-Ford; L: negative-cycle check; M: Floyd-Warshall; N: Part 3 benchmark and harness), each marked done with its files and tests, plus the conventions (toward-target trees, −∞, statuses, fixed point).

#### 26. `README.md` (Modify, only where wrong), commit 14

*   **Type / Scope**: project readme.
*   **Proposed Changes**: only statements made false by Part 3 are fixed (program list, build targets).

#### 27. `data/azure_part3/`, `docs/results/part3_tables.md` (Create), commit 15

*   **Type / Scope**: results.
*   **Proposed Changes**: fetched by `azure_part3.sh fetch` and built by `report_part3.py`; committed only after the owner reviews them.

### Verification Plan

*   **Compilation/Build**: `make` and `make benchmark-weighted` unchanged; `make benchmark-part3` from commit 11: no new warnings, `-DNDEBUG` builds clean.
*   **Unit Tests**: `make test` ends in `Status: SUCCESS!` after every commit on macOS; the Azure `setup` step runs it on Linux (GCC 13, `_GLIBCXX_ASSERTIONS`, ASan + UBSan) before any measurement.
*   **Program and harness tests**: `make test-part3` after commits 12 and 13; after commit 11, a smoke run of `build/benchmark_part3` on `tests/data/weighted_small.txt`.
*   **Negative tests**: each `static_assert(!…)`, local-concept rejection and guard test is run once with its guard removed to see it fail, then restored; this is recorded in the commit message body.
*   **Calibration (on Azure, before the full run)**: `azure_part3.sh calibrate` prints the c·n³ fit and the VM-time estimate; the owner approves it before `start`.
*   **Acceptance**: for each of grafo_W_1 … W_5, `paths_part3.csv` has the 3 Bellman-Ford rows and, on W_1 and W_2, the 20 Floyd-Warshall rows, each `ok`, `unreachable` or `negative_cycle`; `results_part3.csv` has the negative-cycle answer once per graph, the Bellman-Ford and Floyd-Warshall times (`ok`, `fewer_runs` or `extrapolated` with the formula), the all-pairs comparison (`ok` where fully measured, otherwise `extrapolated`), the three optimization variants on W_1 and W_2 with their rounds and relaxations, `fw_vs_bf` `ok` on W_1 and W_2, `no_memory` for Floyd-Warshall on W_3 to W_5 and `skipped` matrix rows; no `mismatch` and no `error` rows.
