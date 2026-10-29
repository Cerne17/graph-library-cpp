#pragma once

#include "graph/concepts.hpp"
#include "graph/types.hpp"

#include <concepts>
#include <vector>

namespace gl::algorithms {
// Result of a single-source shortest-path run. Both vectors are sized n + 1;
// index 0 is unused.
template<std::floating_point W>
struct shortest_paths
{
  vertex_id source = 0;
  std::vector<W> dist;           // dist[v], +inf if v is unreachable
  std::vector<vertex_id> parent; // parent[v], 0 for the source and unreachable
};

namespace detail {
// Preconditions shared by every Dijkstra version.
// - source in 1..n: assert (programmer error).
// - no negative weight: throws std::invalid_argument, checked in O(1) with
//   g.has_negative_weight().
template<weighted_graph G>
void check_dijkstra_input(const G& g, vertex_id source);

// Starting state of every Dijkstra version: dist = +inf and parent = 0 for
// every vertex, except dist[source] = 0.
template<weighted_graph G>
shortest_paths<typename G::weight_type> init_shortest_paths(const G& g,
                                                            vertex_id source);
} // namespace detail
} // namespace gl::algorithms
