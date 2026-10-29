# Agent instructions

C++ graph library for COS242 (Graph Theory, UFRJ). Part 1 (unweighted graphs)
is delivered; Part 2 (weighted graphs, two Dijkstra implementations, case
studies) is in progress on the branch `feature/generalization`.

Read `docs/CONVENTIONS.md` before writing code. It is binding. Human onboarding
lives in `docs/ONBOARDING.md`.

## Two codebases — know which one you are touching

- **Legacy (Part 1):** `include/graph.hpp`, `src/`, `app/`, `analysis/`.
  Runtime polymorphism, camelCase. Delivered and graded. **Do not modify it**
  unless the task explicitly says so.
- **New (Part 2):** `include/graph/`. Header-only C++23 templates, concepts,
  snake_case, namespaces `gl`, `gl::algorithms` and `gl::io`. All new work
  goes here.

## Build and test

Run everything from the repository root.

```bash
make test        # builds and runs all doctest tests; must end in "Status: SUCCESS!"
make             # legacy debug build (build/graphs)
make benchmark   # legacy optimized benchmark
make clean       # removes build/
```

- `make test` must pass before you report a task as done. Run it after every
  meaningful change, not only at the end.
- The test build uses `-fsanitize=undefined` plus both standard libraries'
  debug modes (`_LIBCPP_HARDENING_MODE_DEBUG` and `_GLIBCXX_ASSERTIONS`), and
  adds `-fsanitize=address` on Linux only. On macOS, AddressSanitizer hangs at
  startup with this toolchain: never enable it there.
- New test files `tests/test_*.cpp` are picked up automatically. New headers in
  `include/graph/` or one subfolder deep are tracked automatically.
- Large graph inputs `data/grafo_*.txt` are not in the repository. Do not
  commit them, and do not commit anything under `build/`.

## Architecture (new library)

- `types.hpp`: `vertex_id` (uint32), `edge_index` (uint64), `edge`,
  `weighted_edge<W>`. Vertices are **1-indexed**; index 0 is unused.
- `policies.hpp`: tag types `undirected`/`directed`, `unweighted`/`weighted<W>`,
  and the concepts `direction_policy`, `weight_policy`.
- `concepts.hpp`: `graph_like` (`vertex_count()`, `for_each_neighbor(u, f)`) and
  `weighted_graph` (adds `for_each_edge(u, f)` calling `f(target, weight)`,
  `has_negative_weight()`, plus `G::weight_type`). Algorithms are free function templates constrained by
  these concepts — never by a concrete storage type.
Namespaces: `gl` for everything below except `algorithms/*`
(`gl::algorithms`) and `io/*` (`gl::io`). Helpers go in the nested `detail`.

- `storage/csr.hpp`: `csr_storage<D, W>`, built only via
  `from_edges(n, edges)`. Neighbors are sorted ascending; weights are sorted
  together with their targets. Weight members exist only when weighted (via
  `if constexpr`, trailing `requires W::is_weighted`, and
  `[[no_unique_address]]` / empty-base tricks). Unweighted storage must stay
  zero-overhead.
- `storage/matrix.hpp`: `matrix_storage<D, W>`, same interface as CSR plus
  `has_edge(u, v)`. Unweighted: a bitset; weighted: (n + 1)² cells, `+inf` for
  "no edge", parallel edges keep the minimum weight.
- `graph.hpp`: user-facing aliases `csr_graph<D, W>` and `matrix_graph<D, W>`.
- `io/edge_list.hpp`: `io::read_edge_list<W>(path)` →
  `io::edge_list<W>{n, edges}`. Throws `std::runtime_error` with
  `file:line: message` on bad input.
- `io/load_graph.hpp`: `io::load_graph<G>(path)` = `read_edge_list` +
  `G::from_edges`.
- `io/label_map.hpp`: `io::label_map` (`name_of(v)`, `id_of(name)` with a
  transparent `string_view` lookup) and `io::read_label_map(path)` for
  `id,name` files, ids 1..n in order.
- `heap.hpp`: `indexed_heap<K, Compare>` with a position map and
  `update_key`; `indexed_min_heap<K>` alias. `comp_(a, b)` true means `a` is
  closer to the root (opposite of `std::priority_queue`).
- `algorithms/search.hpp`, `components.hpp`, `distance.hpp`, `stats.hpp`:
  Part 1's BFS, DFS, components, distance, diameters and degree statistics,
  over `graph_like`. `tests/test_algorithms_vs_legacy.cpp` checks them against
  Part 1, which the test binary links.
- Part 2: Dijkstra and paths (tasks E–H in `docs/ONBOARDING.md` §7).
  `tests/test_dijkstra.cpp` runs every case for all three versions;
  `tests/test_dijkstra_differential.cpp` checks them on random graphs against
  the shortest-path optimality conditions and BFS levels.
  - `algorithms/shortest_paths.hpp`: `shortest_paths<W>{source, dist,
    parent}`, 1-indexed; unreachable means `dist = +inf`, `parent = 0`.
    `detail::check_dijkstra_input` (asserts the source, throws
    `std::invalid_argument` on negative weights via `has_negative_weight()`)
    and `detail::init_shortest_paths`, shared by every version.
  - `algorithms/dijkstra_array.hpp` (`dijkstra_array`, O(n² + m)),
    `dijkstra_heap.hpp` (`dijkstra_heap`, `indexed_min_heap` +
    `update_key`, O((n + m) log n)), `dijkstra_lazy.hpp` (`dijkstra_lazy`,
    `std::priority_queue` with stale-entry skipping, O((n + m) log m)).
    `dijkstra.hpp` includes all three.
  - `algorithms/path.hpp`: `reconstruct_path(tree, target)` for any
    `parent_tree` (`search_tree` or `shortest_paths`).

Ownership: `storage/matrix.hpp` and the migrated Part 1 algorithms in
`include/graph/algorithms/` belong to the project owner. **Do not edit those
files, or `concepts.hpp`, unless the task says so.** Changes to
`concepts.hpp` affect everyone.

## Rules for writing code

- Follow `docs/CONVENTIONS.md`: standard-library naming (`snake_case`,
  `_type` aliases, `is_` flags), the namespace of the header's folder closed
  with its full name (`} // namespace gl::io`), `#pragma once`, include paths
  from `include/`
  (`"graph/..."`), include what you use, no `using namespace` in headers.
- Class layout: `private` section first, then `public`. Structs: `public`
  first. Name parameters in declarations. Light constructors and one-line
  members are defined inside the class.
- Preconditions (programmer errors) → `assert` with a message
  (`"function: what went wrong"`). Bad external input → exceptions.
- Use `vertex_id` for vertices and `edge_index` for positions in edge arrays.
  Read untrusted integers into `long long`, validate, then convert.
- Non-template functions defined in headers must be `inline`. Internal helpers
  go in `namespace detail`.
- A member function signature must not name a type that only exists for some
  template arguments (e.g. `typename W::weight_type` in a weighted-only
  member). Use an `auto` return type and build the type in the body.
- Prefer the standard library over hand-rolled code (`std::ranges`, `<bit>`,
  `std::span`). Do not add third-party dependencies; doctest in `third_party/`
  is the only one.
- Keep changes scoped to the task. Do not reformat or rename unrelated code.

## Rules for tests

- Every new header gets a `tests/test_<name>.cpp` that includes it. A header no
  `.cpp` includes is never compiled.
- Compile-time facts: file-scope `static_assert`, one fact per assertion.
  Runtime behavior: `TEST_CASE` with `CHECK`/`REQUIRE`.
- Every concept or `requires` clause gets a negative test: a type rejected for
  exactly one reason. Absence of a member is tested with a small local concept
  wrapping a `requires` expression.
- Template member bodies are only compiled when called: every public member
  must be exercised by at least one test.
- Expected values come from hand computation or an independent reference
  (brute force), never from running the code under test.
- Compare computed floating-point values with `doctest::Approx`; exact `==` only
  for copied, uncomputed values.
- Randomized tests use a fixed seed (`std::mt19937 rng(<seed>)`).
- Small input files go in `tests/data/`; tests reference them as
  `tests/data/<file>`.
- Before claiming a negative test works, confirm it fails when the guarded
  check is removed, then restore the check.

## Git

- Branch from `feature/generalization`; open PRs into it, never into `main`.
- Commit messages: `type(scope): description`, e.g.
  `feat(storage): ...`, `test(io): ...`, `refactor(heap): ...`, `fix: ...`.
- One task per branch/PR. Never commit `build/`, `.DS_Store`, or
  `data/grafo_*.txt`.

## When unsure

Architecture decisions (new concepts, changes to public interfaces, new
policies, changes to existing headers' contracts) belong to the project owner.
Stop and ask instead of deciding.
