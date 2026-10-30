#pragma once

#include "graph/concepts.hpp"
#include "graph/types.hpp"

#include <cassert>
#include <concepts>
#include <limits>
#include <stdexcept>
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
// - source in 1..n: assert (programmer error). [[maybe_unused]]: the assert
//   is its only use, so release builds (-DNDEBUG) would warn otherwise.
// - no negative weight: throws std::invalid_argument, checked in O(1) with
//   g.has_negative_weight().
template<weighted_graph G>
void check_dijkstra_input(const G& g, [[maybe_unused]] vertex_id source)
{
  assert(source >= 1 && source <= g.vertex_count() &&
         "dijkstra: source out of range");
  if (g.has_negative_weight()) {
    throw std::invalid_argument("dijkstra: graph has a negative edge weight");
  }
}

// Starting state of every Dijkstra version: dist = +inf and parent = 0 for
// every vertex, except dist[source] = 0.
template<weighted_graph G>
shortest_paths<typename G::weight_type> init_shortest_paths(const G& g,
                                                            vertex_id source)
{
  using W = typename G::weight_type;
  const vertex_id n = g.vertex_count();

  shortest_paths<W> sp{ source,
                        std::vector<W>(n + 1, std::numeric_limits<W>::infinity()),
                        std::vector<vertex_id>(n + 1, 0) };
  sp.dist[source] = W{ 0 };
  return sp;
}
} // namespace detail
} // namespace gl::algorithms
