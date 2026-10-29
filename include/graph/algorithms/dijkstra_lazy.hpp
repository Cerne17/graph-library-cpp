#pragma once

#include "graph/algorithms/shortest_paths.hpp"
#include "graph/concepts.hpp"
#include "graph/types.hpp"

#include <functional>
#include <queue>
#include <utility>
#include <vector>

namespace gl::algorithms {
// Dijkstra with std::priority_queue and lazy deletion: a relaxation pushes a
// new (dist, vertex) entry instead of updating the old one, and a popped entry
// whose dist is larger than dist[vertex] is stale and skipped.
// O((n + m) log m) time, O(m) extra space (the stale entries).
// The baseline that shows what the indexed heap buys.
// Throws std::invalid_argument if g has a negative weight.
template<weighted_graph G>
shortest_paths<typename G::weight_type> dijkstra_lazy(const G& g,
                                                      vertex_id source)
{
  using W = typename G::weight_type;
  detail::check_dijkstra_input(g, source);
  auto sp = detail::init_shortest_paths(g, source);

  using entry = std::pair<W, vertex_id>; // (dist, vertex): compares dist first
  std::priority_queue<entry, std::vector<entry>, std::greater<entry>> queue;
  queue.push({ W{ 0 }, source });

  while (!queue.empty()) {
    const auto [d, u] = queue.top();
    queue.pop();
    if (d > sp.dist[u]) { // stale: u was settled with a smaller dist
      continue;
    }

    g.for_each_edge(u, [&](vertex_id v, W w) {
      if (d + w < sp.dist[v]) {
        sp.dist[v] = d + w;
        sp.parent[v] = u;
        queue.push({ sp.dist[v], v });
      }
    });
  }
  return sp;
}
} // namespace gl::algorithms
