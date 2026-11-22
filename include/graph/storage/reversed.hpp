#pragma once

#include "graph/concepts.hpp"
#include "graph/policies.hpp"
#include "graph/storage/csr.hpp"
#include "graph/types.hpp"

#include <utility>
#include <vector>

namespace gl {
template<weighted_graph G>
  requires G::direction_type::is_directed
class reversed_graph;

// True only for reversed_graph specializations.
template<typename G>
inline constexpr bool is_reversed_graph = false;

template<weighted_graph H>
  requires H::direction_type::is_directed
inline constexpr bool is_reversed_graph<reversed_graph<H>> = true;

template<weighted_graph G>
  requires G::direction_type::is_directed && (!is_reversed_graph<G>)
reversed_graph<G> reversed(const G& g);

// A directed graph with every arc of G turned around (u -> v of weight w
// becomes v -> u of weight w). A distinct type, so code that needs "the
// reversal of g" cannot be given g itself by mistake. Built only by
// gl::reversed(g), through csr_storage::from_edges: O(n + m log d_max) time
// (the per-vertex sort in from_edges) and a temporary copy of the m arcs, then
// the CSR alone (n + 2 offsets, m targets, m weights).
template<weighted_graph G>
  requires G::direction_type::is_directed
class reversed_graph
{
private:
  using arcs_type = csr_storage<directed, typename G::weighting_type>;

  arcs_type arcs_;

  explicit reversed_graph(arcs_type arcs)
    : arcs_(std::move(arcs))
  {
  }

public:
  using direction_type = directed;
  using weighting_type = typename G::weighting_type;
  using weight_type = typename G::weight_type;

  vertex_id vertex_count() const { return arcs_.vertex_count(); }

  template<typename F>
  void for_each_neighbor(vertex_id u, F&& f) const
  {
    arcs_.for_each_neighbor(u, std::forward<F>(f));
  }

  // f(target, weight) for every reversed arc out of u, in ascending target.
  template<typename F>
  void for_each_edge(vertex_id u, F&& f) const
  {
    arcs_.for_each_edge(u, std::forward<F>(f));
  }

  bool has_negative_weight() const { return arcs_.has_negative_weight(); }

  template<weighted_graph H>
    requires H::direction_type::is_directed && (!is_reversed_graph<H>)
  friend reversed_graph<H> reversed(const H& g);
};

// Rejects a graph that is already reversed: reversing it back would silently
// turn distances to t into distances from t. An undirected graph is its own
// reversal and is rejected too.
template<weighted_graph G>
  requires G::direction_type::is_directed && (!is_reversed_graph<G>)
reversed_graph<G> reversed(const G& g)
{
  using W = typename G::weight_type;
  const vertex_id n = g.vertex_count();

  std::vector<weighted_edge<W>> arcs;
  for (vertex_id u = 1; u <= n; ++u) {
    g.for_each_edge(u, [&](vertex_id v, W w) { arcs.push_back({ v, u, w }); });
  }
  return reversed_graph<G>(
    csr_storage<directed, typename G::weighting_type>::from_edges(n, arcs));
}
} // namespace gl
