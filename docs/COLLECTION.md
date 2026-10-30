# Part 2 graph collection

`analysis/collect.py` drives `build/collect_stats`, using the new `gl` library
for the Part 2 weighted datasets. Topology statistics and BFS/DFS ignore edge
weights; Dijkstra uses them. Part 1 datasets are not required. The collector
also supports unweighted input through a custom config. New measurements are tagged
`gl_header_only` and must not be presented as legacy measurements.

## Build and run

Only C++23, make and Python 3's standard library are required. macOS and Linux
are supported. Run from the repository root:

```bash
make collector
make test-collector
make test

# Structural statistics and visualization exports, without long benchmarks:
python3 analysis/collect.py --only grafo_W_3 --phases stats

# Worksheet paths and a short benchmark run:
python3 analysis/collect.py --only grafo_W_3 --phases cases,timing --k 3 --array-work 0

# All six Part 2 datasets, all phases, 100 random sources:
python3 analysis/collect.py
```

`config_collect.csv` lists only the six Part 2 datasets: W_3 through W_7 and the
collaboration network. Columns are `graph,weighting,representations,labels,source`.
Graph and labels are file stems relative to `--data`; input can be `.txt` or
`.txt.gz`. If both exist, `.txt` wins. Weighting is `unweighted` or `weighted`;
representations are `csr`, `matrix`, or `csr|matrix`. Source is a vertex ID,
or an exact name if labels are supplied. An empty source defaults to vertex
`min(10,n)`. In a labels dataset, the weighted cases phase expects the five
researchers named by the worksheet.

All selected input files must exist before collection starts. Use repeated
`--only` options to select a subset. The collector does not download inputs.

Options include `--config`, `--data`, `--out`, `--binary`, `--tmp`, `--phases`,
`--seed`, `--k`, `--sample-nodes`, `--sample-edges`, `--exact-work`, `--array-work`,
`--timeout`, `--memory-limit-gb`, and `--force`.

## Resource budgets and failures

- Default timeout is 3,600 seconds **per job**, including graph loading but
  excluding the shared decompression/checksum step and output compression.
- Exact hop diameter is attempted only when `n*(n+2m) <= --exact-work`
  (default 100,000,000). Zero disables it. Double sweep supplies an explicitly
  labelled lower bound on the largest component's hop diameter. Neither result
  is a weighted diameter.
- Array Dijkstra runs only when `n*n <= --array-work` per source (default
  10,000,000,000). Zero disables it for nonempty graphs. This work estimate is
  separate from the wall-clock timeout and the requested number of sources.
- Memory is checked using a conservative estimate of input-vector capacity,
  graph storage and algorithm workspace. A weighted matrix uses `(n+1)^2*8`
  bytes; the unweighted matrix uses bits. Default budget is 80% of available
  Linux memory (including the common cgroup v2 limit) or 60% of physical macOS
  memory. Override with `--memory-limit-gb` on a suitably sized machine.
- Memory estimates are a preflight policy, **not a hard RSS limit or proof of
  allocation failure**. Jobs may be conservatively skipped despite fitting;
  actual memory allocation failures are recorded as errors. Adjust the budget
  deliberately, rather than forcing large matrices on a laptop.
- Skipped jobs get `no_memory` or `budget_exceeded` with their reason and
  settings. Timeouts/errors retain stderr and any partial exports, return a
  nonzero runner exit code and are retried on the next run. Partials are never
  included in aggregate tables. Dijkstra with any negative edge or discarded
  negative loop writes `not_implemented` rows with no timing/distance.
- Gzip is fully decompressed to `--tmp`, CRC-checked and hashed before C++ reads
  it. Temporary input is removed on success or failure. The temporary filesystem
  needs room for the largest decompressed graph, and output needs room for the
  plain per-vertex CSV during streaming compression.

## Resume and provenance

Outputs default to `data/collected/` (Git-ignored). Each graph has `input.json`
and one directory per job: `stats`, `cases`, and
`timing_<representation>_<algorithm>`. A job has `job.json`, exports and a
`stderr.log`. Jobs are staged in hidden directories and then published. An
interrupted publication can recover the previous directory on the next run.

Checkpoints include the plaintext SHA-256 of graph and labels, collector binary
SHA-256, runner SHA-256, schema version, host identity, budgets, sample settings
and source.
The runner freezes a private executable copy for each invocation and holds an
exclusive writer lock on the output directory. Export hashes are checked on resume. Changed inputs/settings/binaries or a
missing/changed export cause a rerun. `--force` explicitly reruns matching jobs.
Changing `--k` currently invalidates every selected phase, including stats;
checkpoints intentionally favor correctness over minimum recomputation.

Metadata records UTC timestamps, Git commit and working-tree status, platform,
CPU count, observed compiler, host and invocation. The observed compiler is the
one available at run time; the binary hash identifies the executable actually
used. This avoids claiming to know the build toolchain of a copied executable.

Root `graph_summary.csv` and `benchmark_summary.csv` are rebuilt from successful
current-input checkpoints, never appended. They can be regenerated by rerunning
the same command; successful jobs are reused. There is one retained result per
job, not a history of repeated experiments. Keep distinct output directories
for experiments or machines you want to compare.

## Export schema

### Structural exports (`stats/`)

| File | Columns/content |
|---|---|
| `summary.csv` | `metric,value`: n, retained and input edges, loops, parallel edges, unique edges, simple density, degree min/max/mean/median, isolates, components, weight min/max/mean, negative/zero edge counts |
| `degree_histogram.csv` | `degree,vertices`, exact frequencies |
| `components.csv` | `component,vertices,root`, sorted by decreasing size; IDs follow discovery order |
| `component_histogram.csv` | `vertices,components`, exact frequencies of component sizes |
| `vertices.csv.gz` | `id,degree,component`, every vertex including isolates |
| `weight_histogram.csv` | Weighted only: `lower,upper,edges`, 50 linear bins; final bin includes its upper endpoint |
| `diameter.csv` | `method,scope,distance_kind,value,status,work_estimate` |

All IDs remain 1-indexed. Self-loops are validated, dropped and counted, matching
the new library; this can change degrees relative to the delivered legacy
results. CSR retains parallel edges, so exported degree and weight distributions
count each retained edge. `parallel_edges` counts copies beyond the first edge
per unordered pair, regardless of weight. `unique_edges` and `density_simple`
ignore those copies. Matrices collapse duplicates (minimum weight for weighted
input), so structural summaries are collected once from CSR, not reinterpreted
as matrix degree statistics. Undirected edges are counted once.

Exact statistics are computed on the **full graph**, not on a network sample.
Degree distributions support histograms, CDF/CCDF and degree-versus-component
plots. Component tables support size distributions and giant-component shares.
The weight histogram uses linear bins; constant weights occupy the first bin,
with equal bounds, and edgeless graphs have no weight-bin rows.

### Visualization exports (`stats/`)

Both `neighborhood_*` and `overview_*` contain:

- `*_nodes.csv.gz`: `id,label,degree,component,component_size`.
- `*_edges.csv.gz`: `source,target,weight`; unweighted edges have weight 1.
- `*_sample.csv`: method, seed, actual node count, node limit, total induced
  edge count, exported edge count, edge limit and starting vertices.

Neighborhood is a bounded BFS around the configured source. Overview starts
from up to 16 component roots spread across component-size ranks, then expands
with BFS. It includes small components where the limits allow. Defaults are
1,000 nodes and 4,000 edges per view. Set `--sample-nodes 0` to disable samples.
If the induced edge count exceeds the edge limit, a fixed-seed reservoir chooses
which edges to export. Original weights and parallel edges are retained.

These are **illustrations, not unbiased estimates** of the full network. A
sample degree should be read from the exported full-graph `degree`, not inferred
from the displayed edges. Omitted edges may make a view appear disconnected.
The collector provides topology, not layout coordinates. Worksheet paths in
`cases.csv` can be overlaid separately; their vertices are not guaranteed to fit
within either bounded network sample.

### Measurements and worksheet cases

`timings.csv` saves `algorithm,source,seed,elapsed_micros,reachable_vertices,status`
for each run. All representations/algorithms use the same ordered distinct
sources for the same n, k, seed and C++ standard-library implementation. Actual
source IDs are exported, allowing cross-toolchain comparisons to verify this.
The source's component size is `reachable_vertices`; reaching all vertices is
not assumed. BFS/DFS run on all graphs; Dijkstra only on weighted graphs.

Timing encloses the algorithm call and its allocations, excluding I/O,
reachability counting and export. Algorithms run in separate jobs; their
execution order and external machine load can affect comparisons. Raw samples
are retained to examine this instead of relying only on averages.

Current RSS is measured by the collector at READY, after releasing the input
edge list and before export/algorithm allocations. A one-vertex process supplies
a per-weighting/per-representation baseline. Both raw and baseline-subtracted
RSS are saved in bytes; subtraction is an estimate and may be noisy for tiny
graphs. This snapshot is **not peak algorithm memory**. On glibc, freed allocator
pages are trimmed before the snapshot; other allocators may retain some pages.

`cases.csv` saves algorithm, source/target IDs and names, parent, distance, hops,
path IDs and status. Part 1 includes the required BFS/DFS parents for roots
1/2/3 and targets 10/20/30, plus the three required pair distances. DFS depths
are hops in its tree, not shortest-path distances, so its `distance` is empty.
Part 2 includes the five required targets from 10, or the exact collaboration
names. Targets beyond n are omitted for small fixtures. Unreachable weighted
targets have `distance=inf` and an empty path; `not_implemented` has no distance.
