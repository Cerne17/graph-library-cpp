#pragma once

#include "graph/algorithms/components.hpp"
#include "graph/algorithms/search.hpp"
#include "graph/concepts.hpp"
#include "graph/types.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace gl::algorithms {
// Number of edges on a shortest u-v path, or -1 if v is unreachable from u.
template<graph_like G>
std::int32_t distance(const G& g, vertex_id u, vertex_id v)
{
  assert(v >= 1 && v <= g.vertex_count() && "distance: v out of range");
  return bfs(g, u).level[v]; // bfs asserts u
}

// Largest finite distance over all pairs: a BFS from every vertex.
// On a disconnected graph, unreachable pairs are skipped, so this is the
// largest diameter among the components.
template<graph_like G>
std::int32_t exact_diameter(const G& g)
{
  const vertex_id n = g.vertex_count();
  std::int32_t diameter = 0;
  for (vertex_id s = 1; s <= n; ++s) {
    const search_tree tree = bfs(g, s);
    diameter = std::max(diameter, std::ranges::max(tree.level));
  }
  return diameter;
}

// Double sweep: BFS from the first vertex of the largest component, then BFS
// from the farthest vertex found (ties go to the highest id), and return that
// vertex's eccentricity.
// A lower bound on exact_diameter: exact on trees, possibly smaller on general
// graphs, and it only looks at the largest component.
template<graph_like G>
  requires(!G::direction_type::is_directed)
std::int32_t approximate_diameter(const G& g)
{
  const vertex_id n = g.vertex_count();
  if (n == 0) {
    return 0;
  }

  const auto components = connected_components(g);
  const search_tree first = bfs(g, components.front().front());

  vertex_id farthest = first.source;
  for (vertex_id v = 1; v <= n; ++v) {
    if (first.level[v] >= first.level[farthest]) {
      farthest = v;
    }
  }

  return std::ranges::max(bfs(g, farthest).level);
}
} // namespace gl::algorithms
