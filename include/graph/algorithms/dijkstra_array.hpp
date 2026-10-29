#pragma once

#include "graph/algorithms/shortest_paths.hpp"
#include "graph/concepts.hpp"
#include "graph/types.hpp"

#include <limits>
#include <vector>

namespace gl::algorithms {
// Dijkstra with a plain array: each round scans all unvisited vertices for the
// one with the smallest dist, then relaxes its edges. Stops early when the
// smallest dist left is +inf (the rest is unreachable).
// O(n^2 + m) time, O(n) extra space. Best on dense graphs (m close to n^2).
// Throws std::invalid_argument if g has a negative weight.
template<weighted_graph G>
shortest_paths<typename G::weight_type> dijkstra_array(const G& g,
                                                       vertex_id source)
{
  using W = typename G::weight_type;
  detail::check_dijkstra_input(g, source);
  auto sp = detail::init_shortest_paths(g, source);

  const vertex_id n = g.vertex_count();
  std::vector<bool> done(n + 1, false);

  for (vertex_id round = 0; round < n; ++round) {
    vertex_id u = 0;
    W best = std::numeric_limits<W>::infinity();
    for (vertex_id v = 1; v <= n; ++v) {
      if (!done[v] && sp.dist[v] < best) {
        best = sp.dist[v];
        u = v;
      }
    }
    if (u == 0) { // the smallest dist left is +inf: the rest is unreachable
      break;
    }

    done[u] = true;
    g.for_each_edge(u, [&](vertex_id v, W w) {
      if (!done[v] && sp.dist[u] + w < sp.dist[v]) {
        sp.dist[v] = sp.dist[u] + w;
        sp.parent[v] = u;
      }
    });
  }
  return sp;
}
} // namespace gl::algorithms
