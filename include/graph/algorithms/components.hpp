#pragma once

#include "graph/concepts.hpp"
#include "graph/types.hpp"

#include <algorithm>
#include <queue>
#include <utility>
#include <vector>

namespace gl::algorithms {
// Connected components, one BFS per unvisited vertex.
// Returns the components largest first; inside each, vertices in BFS visiting
// order. The sort is stable, so equal-sized components keep discovery order
// (by smallest vertex id).
// Directed graphs have weak and strong components instead, so they are
// rejected at compile time.
template<graph_like G>
  requires(!G::direction_type::is_directed)
std::vector<std::vector<vertex_id>> connected_components(const G& g)
{
  const vertex_id n = g.vertex_count();
  std::vector<bool> visited(n + 1, false);
  std::vector<std::vector<vertex_id>> components;
  std::queue<vertex_id> queue;

  for (vertex_id s = 1; s <= n; ++s) {
    if (visited[s]) {
      continue;
    }

    std::vector<vertex_id> component;
    visited[s] = true;
    component.push_back(s);
    queue.push(s);

    while (!queue.empty()) {
      const vertex_id u = queue.front();
      queue.pop();

      g.for_each_neighbor(u, [&](vertex_id v) {
        if (!visited[v]) {
          visited[v] = true;
          component.push_back(v);
          queue.push(v);
        }
      });
    }
    components.push_back(std::move(component));
  }

  std::ranges::stable_sort(components, [](const auto& a, const auto& b) {
    return a.size() > b.size();
  });
  return components;
}
} // namespace gl::algorithms
