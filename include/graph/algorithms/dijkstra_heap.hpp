#pragma once

#include "graph/algorithms/shortest_paths.hpp"
#include "graph/concepts.hpp"
#include "graph/heap.hpp"
#include "graph/types.hpp"

#include <limits>

namespace gl::algorithms {
// Dijkstra with gl::indexed_min_heap: each vertex enters the heap at most
// once, and a relaxation that improves dist[v] calls update_key(v, ...) on it.
// O((n + m) log n) time, O(n) extra space. Best on sparse graphs.
// Throws std::invalid_argument if g has a negative weight.
template<weighted_graph G>
shortest_paths<typename G::weight_type> dijkstra_heap(const G& g,
                                                      vertex_id source)
{
  using W = typename G::weight_type;
  detail::check_dijkstra_input(g, source);
  auto sp = detail::init_shortest_paths(g, source);

  indexed_min_heap<W> heap(g.vertex_count());
  heap.push(source, W{ 0 });

  while (!heap.empty()) {
    const vertex_id u = heap.pop(); // dist[u] is final from here on
    g.for_each_edge(u, [&](vertex_id v, W w) {
      const W nd = sp.dist[u] + w;
      if (nd < sp.dist[v]) { // never true for a popped v: weights are >= 0
        const bool pushed = sp.dist[v] != std::numeric_limits<W>::infinity();
        sp.dist[v] = nd;
        sp.parent[v] = u;
        if (heap.contains(v)) {
          heap.update_key(v, nd);
        } else if (!pushed) {
          heap.push(v, nd);
        }
      }
    });
  }
  return sp;
}
} // namespace gl::algorithms
