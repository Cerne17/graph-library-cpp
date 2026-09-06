# graph-library-cpp

An undirected graph library in C++17 with no external dependencies, built around
a single abstract interface with two interchangeable representations —
adjacency list and adjacency matrix — so the cost of a representation choice is
something you can measure rather than assume.

Written for COS232 (Teoria dos Grafos, UFRJ).

## What's in it

- **Two representations** behind one `Graph` interface: `AdjacencyList` and `AdjacencyMatrix`.
- **Traversals**: BFS and DFS, both returning a search tree of parents and levels.
- **Connected components**, computed either breadth-first or depth-first.
- **Distances** between vertex pairs.
- **Diameter**, exact (all-pairs BFS) and approximate (double sweep), with a work
  budget that falls back to the approximation when the exact computation is too
  expensive.
- **Degree statistics**: min, max, mean, median.
- A **benchmark driver** that reports timing and peak memory, and a Python
  harness that drives it across a batch of graphs.

Every public function in [`include/graph.hpp`](include/graph.hpp) carries a
docstring with its arguments, its return value, and its cost **for both
representations**.

## Building

```bash
make                # app/graphs           debug build, -g, no optimisation
make release        # app/graphs_release   optimised, -O2
make benchmark      # app/benchmark        optimised, -O2
make clean
```

Run everything from the repository root — output paths are relative to it.

## Using it

`app/graphs` writes the CSV outputs for one graph, or for a batch:

```bash
./app/graphs_release                        # no arguments: reads config.csv
./app/graphs_release data/graph.txt both    # one graph: both | list | matrix
```

With no arguments it walks `config.csv`, one `graph,representation` row per
graph, resolving `<name>` to `data/<name>.txt`. A graph that fails is reported
and skipped rather than aborting the run.

`app/benchmark` produces the measurements:

```bash
./app/benchmark data/graph.txt list time     # average BFS and DFS microseconds
./app/benchmark data/graph.txt list mem      # holds the graph so RSS can be sampled
./app/benchmark data/graph.txt list report   # distances, components, diameters
./app/benchmark data/graph.txt list all      # all of the above, one process, one load
python analysis/harness.py                   # drives the above over config.csv
```

The harness needs `pandas` and `psutil`.

## Input format

First line is the vertex count; every line after it is one edge as a pair of
vertex ids.

```
5
1 2
2 5
5 3
4 5
1 5
```

Vertices are **1-indexed**. Every internal vector is sized `n+1` with index 0
left unused, so a loop starting at 0 reads padding.

## Output

Each run writes four CSVs into `data/`, named after the input graph:

| file | one row per | columns |
| --- | --- | --- |
| `stats_<name>.csv` | representation | counts, degree statistics, components, diameters |
| `bfs_tree_<name>.csv` | vertex | `node, parent, level` |
| `dfs_tree_<name>.csv` | vertex | `node, parent, level` |
| `components_<name>.csv` | vertex | `node, componentId` |

## Complexity

`n` is the vertex count, `m` the edge count, `k` the number of components.

| operation | adjacency list | adjacency matrix |
| --- | --- | --- |
| construction | O(n) | O(n²) |
| `addEdge` | O(1) amortised | O(1) |
| `degree(u)` | O(1) | **O(n)** |
| `neighbors(u)` | O(deg u) | **O(n)** |
| `finalize` | O(m log n) | O(1) |
| `readGraph` | O(n + m log n) | O(n² + m) |
| `bfs` · `dfs` | O(n + m) | **O(n²)** |
| `getDistance` | O(n + m) | O(n²) |
| components | O(n + m + k log k) | O(n²) |
| `getApproximateDiameter` | O(n + m) | O(n²) |
| `getExactDiameter` | O(n·(n+m)) | **O(n³)** |
| memory | O(n + m) ints | O(n²) bytes |

The whole matrix column follows from one line: `neighbors()` scans an entire row
regardless of how few neighbours a vertex has. Anything that is O(n + m) against
a list is O(n²) against a matrix. Measured on a 10,000-vertex graph, one BFS
takes 584 µs on the list and 41,327 µs on the matrix.

Because `getExactDiameter` costs about `n·(n+m)` edge inspections, `computeStats`
takes a budget in those units. Over budget, `exactDiameter` comes back as `-1`
and the graph is described by the approximation alone. Budgeting the product
rather than `n` is what catches dense graphs: two graphs can share a vertex count
and still differ by an order of magnitude in cost.

## Conventions worth knowing

- **`-1` means undefined**, consistently: an unreachable vertex's `level`, a
  `getDistance` across components, a skipped `exactDiameter`.
- **A DFS `level` is depth in the DFS tree**, not distance in the graph. Use
  `bfs` or `getDistance` for distances.
- **`parent` is 0 for the root and for unreachable vertices alike.** Only `level`
  separates the two.
- **Neither representation deduplicates edges.** The list stores multiplicity, so
  a repeated edge raises the degrees and the handshake lemma holds against `m`.
  The matrix write is idempotent, so its degrees describe the underlying simple
  graph while `m` still counts the file — the two disagree on inputs that repeat
  an edge or carry self-loops.
- **`getApproximateDiameter` is a lower bound.** Double sweep is exact on trees
  and an underestimate on general graphs; it also sweeps only the largest
  component, so on a disconnected graph a smaller component may hold the real
  diameter.
- **Component ids are not stable across runs.** Components are sorted by size
  with a non-stable sort, so equal-sized ones can swap places.

## Layout

```
include/graph.hpp     the entire interface, with the docstrings
src/graph.cpp         AdjacencyList and AdjacencyMatrix
src/io.cpp            the input parser and every CSV writer
src/search.cpp        bfs, dfs, and both diameters
src/components.cpp    connected components, breadth-first and depth-first
src/distance.cpp      getDistance
src/stats.cpp         computeStats
app/main.cpp          CLI that produces the output CSVs
app/benchmark.cpp     timing, memory and report modes
analysis/harness.py   drives the benchmark over config.csv
```

Start with `include/graph.hpp`, then `src/search.cpp`. The rest follows from
those two.
