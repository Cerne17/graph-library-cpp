#pragma once

#include "graph/concepts.hpp"
#include "graph/storage/reversed.hpp"
#include "graph/types.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <span>
#include <utility>
#include <vector>

namespace gl::algorithms {
// Result of Bellman-Ford toward one target. Vectors are sized n + 1; index 0
// is unused.
template<std::floating_point W>
struct target_paths
{
  vertex_id target = 0;
  std::vector<W> dist;          // dist[v]: shortest v -> target; +inf if v
                                // cannot reach target; -inf if a negative
                                // cycle lies on some v -> target walk
  std::vector<vertex_id> next;  // vertex after v on its path; 0 for the
                                // target and for +inf and -inf vertices
  bool cycle_reaches_target = false; // some dist is -inf
};

// The three variants: the textbook version (every arc, all n rounds),
// optimization 1 (stop after a round with no change), and optimizations 1
// and 2 (also relax only around vertices that changed in the previous round;
// an empty frontier ends the loop).
enum class bellman_ford_variant
{
  textbook,
  stop_when_stable,
  frontier
};

struct bellman_ford_options
{
  bellman_ford_variant variant = bellman_ford_variant::frontier;
};

// Work done, for tests and reports. A relaxation is one arc examined, not
// only one that improved a distance.
struct bellman_ford_stats
{
  std::uint32_t rounds = 0;
  edge_index relaxations = 0;
};

// What bellman_ford_on_reversed accepts: a reversal built by gl::reversed, or
// an undirected graph (its own reversal).
template<typename R>
concept reverse_input = is_reversed_graph<R> || !R::direction_type::is_directed;

namespace detail {
// Synchronous relaxation rounds over rev until nothing changes (non-textbook
// variants) or n rounds have run. Each round reads prev, the distances at the
// end of the previous round, and writes dist; the changed entries are copied
// into prev after the round. initial_frontier is read by the frontier
// variant only. Returns the vertices changed in round n, which only a
// negative cycle can cause, or an empty vector when the loop stopped earlier.
template<weighted_graph R>
std::vector<vertex_id> relax_rounds(const R& rev,
                                    std::vector<typename R::weight_type>& dist,
                                    std::vector<vertex_id>& next,
                                    std::vector<vertex_id> initial_frontier,
                                    bellman_ford_variant variant,
                                    bellman_ford_stats& stats)
{
  using W = typename R::weight_type;
  constexpr W inf = std::numeric_limits<W>::infinity();
  const vertex_id n = rev.vertex_count();

  std::vector<W> prev = dist;
  std::vector<bool> queued(n + 1, false);
  std::vector<vertex_id> frontier = std::move(initial_frontier);
  std::vector<vertex_id> changed;

  const auto relax_from = [&](vertex_id x) {
    rev.for_each_edge(x, [&](vertex_id u, W w) {
      ++stats.relaxations;
      const W candidate = prev[x] + w;
      if (candidate < dist[u]) {
        dist[u] = candidate;
        next[u] = x;
        if (!queued[u]) {
          queued[u] = true;
          changed.push_back(u);
        }
      }
    });
  };

  for (vertex_id round = 1; round <= n; ++round) {
    stats.rounds = round;
    changed.clear();

    if (variant == bellman_ford_variant::frontier) {
      for (const vertex_id x : frontier) {
        relax_from(x);
      }
    } else {
      for (vertex_id x = 1; x <= n; ++x) {
        if (prev[x] != inf) {
          relax_from(x);
        }
      }
    }

    for (const vertex_id u : changed) {
      prev[u] = dist[u];
      queued[u] = false;
    }
    if (changed.empty() && variant != bellman_ford_variant::textbook) {
      return {};
    }
    if (round == n) {
      return changed;
    }
    frontier.swap(changed);
  }
  return {};
}

// Marks every vertex that can reach a seed (BFS over rev from the seeds):
// dist = -inf, next = 0.
template<weighted_graph R>
void mark_minus_infinity(const R& rev,
                         std::span<const vertex_id> seeds,
                         target_paths<typename R::weight_type>& tree)
{
  using W = typename R::weight_type;
  constexpr W minus_inf = -std::numeric_limits<W>::infinity();

  std::vector<bool> seen(rev.vertex_count() + 1, false);
  std::queue<vertex_id> queue;
  for (const vertex_id s : seeds) {
    if (!seen[s]) {
      seen[s] = true;
      queue.push(s);
    }
  }
  while (!queue.empty()) {
    const vertex_id x = queue.front();
    queue.pop();
    tree.dist[x] = minus_inf;
    tree.next[x] = 0;
    rev.for_each_neighbor(x, [&](vertex_id u) {
      if (!seen[u]) {
        seen[u] = true;
        queue.push(u);
      }
    });
  }
}
} // namespace detail

// Bellman-Ford toward target, on rev: gl::reversed(g) for a directed g, or g
// itself when undirected. Lets callers that ask for many targets build the
// reversal once. negative_loops are the negative self-loops read from the file
// (io::edge_list::negative_self_loop_edges), which no graph stores.
// Works with negative weights; negative cycles that reach the target are
// reported in the result (dist = -inf), not thrown. Precondition (assert):
// target in 1..n. O(n * m) worst case.
template<weighted_graph R>
  requires reverse_input<R>
target_paths<typename R::weight_type> bellman_ford_on_reversed(
  const R& rev,
  vertex_id target,
  std::span<const weighted_edge<typename R::weight_type>> negative_loops = {},
  bellman_ford_options options = {},
  bellman_ford_stats* stats = nullptr)
{
  using W = typename R::weight_type;
  const vertex_id n = rev.vertex_count();
  assert(target >= 1 && target <= n && "bellman_ford: target out of range");

  bellman_ford_stats local;
  bellman_ford_stats& st = stats != nullptr ? *stats : local;
  st = {};

  target_paths<W> tree{ target,
                        std::vector<W>(n + 1, std::numeric_limits<W>::infinity()),
                        std::vector<vertex_id>(n + 1, 0),
                        false };
  tree.dist[target] = W{ 0 };

  // 1. the rounds; seeds are the vertices still changing in round n
  std::vector<vertex_id> seeds = detail::relax_rounds(
    rev, tree.dist, tree.next, { target }, options.variant, st);

  // 2. a negative self-loop on a vertex that reaches the target is a
  //    negative cycle the rounds never see
  for (const auto& loop : negative_loops) {
    assert(loop.u == loop.v && "bellman_ford: negative_loops must be self-loops");
    if (loop.weight < W{ 0 } &&
        tree.dist[loop.u] != std::numeric_limits<W>::infinity()) {
      seeds.push_back(loop.u);
    }
  }

  // 3. everything that reaches a seed has distance -inf
  if (!seeds.empty()) {
    detail::mark_minus_infinity(rev, std::span<const vertex_id>(seeds), tree);
    tree.cycle_reaches_target = true;
  }
  return tree;
}

// Shortest paths from every vertex to target. Builds the reversal itself when g
// is directed (O(n + m log d_max)); a reversal goes to bellman_ford_on_reversed
// instead, so it cannot be reversed back by mistake.
template<weighted_graph G>
  requires(!is_reversed_graph<G>)
target_paths<typename G::weight_type> bellman_ford(
  const G& g,
  vertex_id target,
  std::span<const weighted_edge<typename G::weight_type>> negative_loops = {},
  bellman_ford_options options = {},
  bellman_ford_stats* stats = nullptr)
{
  if constexpr (G::direction_type::is_directed) {
    return bellman_ford_on_reversed(
      gl::reversed(g), target, negative_loops, options, stats);
  } else {
    return bellman_ford_on_reversed(g, target, negative_loops, options, stats);
  }
}

// The path v, ..., target, v first. Empty, in O(1) and in every build, when
// dist[v] is +inf or -inf (checked first, so a target on a negative cycle
// gives empty); otherwise {target} when v == target. As a last guard, a walk
// longer than n steps (a corrupt tree) stops and returns empty.
// Precondition (assert): v in 1..n.
template<std::floating_point W>
std::vector<vertex_id> path_to_target(const target_paths<W>& tree, vertex_id v)
{
  assert(v >= 1 && v < tree.next.size() && "path_to_target: vertex out of range");

  constexpr W inf = std::numeric_limits<W>::infinity();
  if (tree.dist[v] == inf || tree.dist[v] == -inf) {
    return {};
  }
  if (v == tree.target) {
    return { v };
  }

  const std::size_t max_vertices = tree.next.size();
  std::vector<vertex_id> path{ v };
  while (path.back() != tree.target) {
    const vertex_id after = tree.next[path.back()];
    if (after == 0 || path.size() == max_vertices) {
      return {};
    }
    path.push_back(after);
  }
  return path;
}

// The whole-graph negative-cycle check.
struct negative_cycle_report
{
  bool found = false;
  std::vector<vertex_id> cycle; // the only witness in the library: arc order
                                // v0 -> v1 -> ... -> v0, v0 not repeated;
                                // empty when !found
};

namespace detail {
// A cycle of the predecessor graph (pred[v] -> v is an arc), as the vertices in
// arc order. Walking n steps back from start, a vertex changed in round n,
// lands on a cycle. Empty if the walk ends at 0 instead.
inline std::vector<vertex_id> pointer_cycle(const std::vector<vertex_id>& pred,
                                            vertex_id start)
{
  const std::size_t n = pred.size() - 1;
  vertex_id x = start;
  for (std::size_t step = 0; step < n && x != 0; ++step) {
    x = pred[x];
  }
  if (x == 0) {
    return {};
  }
  std::vector<vertex_id> cycle{ x };
  for (vertex_id v = pred[x]; v != x; v = pred[v]) {
    cycle.push_back(v);
  }
  std::ranges::reverse(cycle); // the walk went against the arcs
  return cycle;
}
} // namespace detail

// Any negative cycle anywhere in g, including the negative self-loops passed
// in. Run once per graph. O(1) when negative_loops has an entry (witness: the
// loop vertex) or when g has no negative weight (none). O(m) for an undirected
// g with a negative edge (witness: its two ends). Otherwise synchronous rounds
// (frontier variant) on g itself, no reversal needed, from an implicit virtual
// source: every distance starts at 0 and every vertex is in the first
// frontier; a change in round n means a cycle, whose witness comes from the
// predecessor pointers.
template<weighted_graph G>
negative_cycle_report find_negative_cycle(
  const G& g,
  std::span<const weighted_edge<typename G::weight_type>> negative_loops = {},
  bellman_ford_stats* stats = nullptr)
{
  using W = typename G::weight_type;
  const vertex_id n = g.vertex_count();

  bellman_ford_stats local;
  bellman_ford_stats& st = stats != nullptr ? *stats : local;
  st = {};

  for (const auto& loop : negative_loops) {
    assert(loop.u == loop.v &&
           "find_negative_cycle: negative_loops must be self-loops");
    if (loop.weight < W{ 0 }) {
      return { true, { loop.u } };
    }
  }
  if (!g.has_negative_weight()) {
    return {};
  }
  if constexpr (!G::direction_type::is_directed) {
    // u-v-u is a cycle of weight 2w < 0
    for (vertex_id u = 1; u <= n; ++u) {
      vertex_id other = 0;
      g.for_each_edge(u, [&](vertex_id v, W w) {
        if (other == 0 && w < W{ 0 }) {
          other = v;
        }
      });
      if (other != 0) {
        return { true, { u, other } };
      }
    }
    return {};
  } else {
    std::vector<W> dist(n + 1, W{ 0 });
    std::vector<vertex_id> pred(n + 1, 0);
    std::vector<vertex_id> everyone(n);
    for (vertex_id v = 1; v <= n; ++v) {
      everyone[v - 1] = v;
    }
    const std::vector<vertex_id> changed = detail::relax_rounds(
      g, dist, pred, std::move(everyone), bellman_ford_variant::frontier, st);
    if (changed.empty()) {
      return {};
    }
    return { true, detail::pointer_cycle(pred, changed.front()) };
  }
}
} // namespace gl::algorithms
