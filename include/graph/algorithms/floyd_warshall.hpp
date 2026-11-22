#pragma once

#include "graph/concepts.hpp"
#include "graph/types.hpp"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace gl::algorithms {
// The default observer of floyd_warshall: does nothing.
struct no_op_pivot
{
  void operator()(vertex_id, vertex_id) const noexcept {}
};

template<std::floating_point W>
class all_pairs;

// Floyd-Warshall for every pair, with one distance matrix and one predecessor
// matrix updated in place for every k (the memory optimization). Works with
// negative weights; negative_loops are the negative self-loops read from the
// file (io::edge_list::negative_self_loop_edges), which no graph stores.
// on_pivot(k, rows_not_skipped) is called after each k. Every k runs: there is
// no early stop. O(n^3) time, (n + 1)^2 * (sizeof(W) + sizeof(vertex_id)) bytes.
template<weighted_graph G, typename OnPivot = no_op_pivot>
all_pairs<typename G::weight_type> floyd_warshall(
  const G& g,
  std::span<const weighted_edge<typename G::weight_type>> negative_loops = {},
  OnPivot on_pivot = {});

// Result of floyd_warshall. Cell (i, j) of both matrices sits at
// i * (n + 1) + j; row and column 0 are unused, so rows index by vertex id.
template<std::floating_point W>
class all_pairs
{
private:
  vertex_id n_ = 0;
  std::vector<W> dist_;
  std::vector<vertex_id> pred_;      // pred(i, j): vertex before j on i -> j
  std::vector<vertex_id> on_cycle_;  // every k with dist(k, k) < 0

  std::size_t index(vertex_id i, vertex_id j) const
  {
    return static_cast<std::size_t>(i) * (static_cast<std::size_t>(n_) + 1) + j;
  }

  template<weighted_graph G, typename OnPivot>
  friend all_pairs<typename G::weight_type> floyd_warshall(
    const G& g,
    std::span<const weighted_edge<typename G::weight_type>> negative_loops,
    OnPivot on_pivot);

public:
  // Row i as a tree rooted at i: parent is the pred row (pred(i, i) == 0).
  struct row_tree
  {
    vertex_id source;
    std::span<const vertex_id> parent; // size n + 1
    std::span<const W> dist;           // size n + 1
  };

  vertex_id vertex_count() const { return n_; }

  // +inf if j is unreachable from i. Meaningful only if !minus_infinity(i, j).
  W dist(vertex_id i, vertex_id j) const
  {
    assert(i >= 1 && i <= n_ && j >= 1 && j <= n_ &&
           "all_pairs::dist: vertex out of range");
    return dist_[index(i, j)];
  }

  // True when a negative cycle lies on some i -> j walk, so the distance is
  // -inf: some k with dist(k, k) < 0 is reachable from i and reaches j.
  // O(number of such k).
  bool minus_infinity(vertex_id i, vertex_id j) const
  {
    assert(i >= 1 && i <= n_ && j >= 1 && j <= n_ &&
           "all_pairs::minus_infinity: vertex out of range");
    constexpr W inf = std::numeric_limits<W>::infinity();
    for (const vertex_id k : on_cycle_) {
      if (dist_[index(i, k)] < inf && dist_[index(k, j)] < inf) {
        return true;
      }
    }
    return false;
  }

  // Some dist(i, i) < 0: the graph has a negative cycle somewhere.
  bool negative_cycle() const { return !on_cycle_.empty(); }

  // No copy. reconstruct_path(tree_from(i), j) is valid when
  // !minus_infinity(i, j).
  row_tree tree_from(vertex_id i) const&
  {
    assert(i >= 1 && i <= n_ && "all_pairs::tree_from: vertex out of range");
    const std::size_t width = static_cast<std::size_t>(n_) + 1;
    return { i,
             std::span<const vertex_id>(pred_.data() + index(i, 0), width),
             std::span<const W>(dist_.data() + index(i, 0), width) };
  }
  row_tree tree_from(vertex_id i) const&& = delete; // no view into a temporary

  std::size_t bytes() const
  {
    const std::size_t width = static_cast<std::size_t>(n_) + 1;
    return width * width * (sizeof(W) + sizeof(vertex_id));
  }
};

template<weighted_graph G, typename OnPivot>
all_pairs<typename G::weight_type> floyd_warshall(
  const G& g,
  std::span<const weighted_edge<typename G::weight_type>> negative_loops,
  OnPivot on_pivot)
{
  using W = typename G::weight_type;
  constexpr W inf = std::numeric_limits<W>::infinity();
  const vertex_id n = g.vertex_count();
  const std::size_t width = static_cast<std::size_t>(n) + 1;

  all_pairs<W> r;
  r.n_ = n;
  r.dist_.assign(width * width, inf);
  r.pred_.assign(width * width, 0);

  // initial matrices: 0 on the diagonal, the lightest of parallel arcs, and the
  // negative self-loops on the diagonal
  for (vertex_id i = 1; i <= n; ++i) {
    r.dist_[r.index(i, i)] = W{ 0 };
  }
  for (vertex_id u = 1; u <= n; ++u) {
    g.for_each_edge(u, [&](vertex_id v, W w) {
      if (w < r.dist_[r.index(u, v)]) {
        r.dist_[r.index(u, v)] = w;
        r.pred_[r.index(u, v)] = u;
      }
    });
  }
  for (const auto& loop : negative_loops) {
    assert(loop.u == loop.v &&
           "floyd_warshall: negative_loops must be self-loops");
    W& cell = r.dist_[r.index(loop.u, loop.u)];
    if (loop.weight < cell) {
      cell = loop.weight;
    }
  }

  // one kernel: row k is read-only during step k (row i = k is skipped), so its
  // pointers and dist(i, k) are hoisted out of the j loop; a cell is written
  // only when it improves
  for (vertex_id k = 1; k <= n; ++k) {
    const W* const dist_k = r.dist_.data() + r.index(k, 0);
    const vertex_id* const pred_k = r.pred_.data() + r.index(k, 0);
    vertex_id rows = 0;
    for (vertex_id i = 1; i <= n; ++i) {
      if (i == k) {
        continue;
      }
      const W dist_ik = r.dist_[r.index(i, k)];
      if (dist_ik == inf) {
        continue; // no path i -> k: nothing to gain
      }
      ++rows;
      W* const dist_i = r.dist_.data() + r.index(i, 0);
      vertex_id* const pred_i = r.pred_.data() + r.index(i, 0);
      for (vertex_id j = 1; j <= n; ++j) {
        const W candidate = dist_ik + dist_k[j];
        if (candidate < dist_i[j]) {
          dist_i[j] = candidate;
          pred_i[j] = pred_k[j];
        }
      }
    }
    on_pivot(k, rows);
  }

  // a negative cycle may have overwritten the diagonal of pred
  for (vertex_id i = 1; i <= n; ++i) {
    r.pred_[r.index(i, i)] = 0;
    if (r.dist_[r.index(i, i)] < W{ 0 }) {
      r.on_cycle_.push_back(i);
    }
  }
  return r;
}
} // namespace gl::algorithms
