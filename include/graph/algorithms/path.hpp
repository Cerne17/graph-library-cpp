#pragma once

#include "graph/types.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <vector>

namespace gl::algorithms {
// Anything rooted at a source with a 1-indexed parent array: parent[source]
// == 0, parent[v] == 0 for unreached v. search_tree (BFS/DFS), shortest_paths
// (Dijkstra) and all_pairs::row_tree (Floyd-Warshall, whose parent is a
// std::span) qualify.
template<typename T>
concept parent_tree = requires(const T& t) {
  { t.source } -> std::convertible_to<vertex_id>;
  requires std::ranges::random_access_range<decltype((t.parent))>;
  requires std::ranges::sized_range<decltype((t.parent))>;
  requires std::same_as<std::ranges::range_value_t<decltype((t.parent))>,
                        vertex_id>;
};

// Vertices on the tree path from tree.source to target, source first and
// target last. {source} when target == source; empty when target is
// unreachable (parent[target] == 0 and target != source). A walk that has
// not reached the source after parent.size() steps (a corrupt tree with a
// cycle) also gives empty, so it can never loop forever.
// O(path length). Precondition (assert): target in 1..n.
template<parent_tree T>
std::vector<vertex_id> reconstruct_path(const T& tree, vertex_id target)
{
  assert(target >= 1 && target < tree.parent.size() &&
         "reconstruct_path: target out of range");

  if (target == tree.source) {
    return { tree.source };
  }
  if (tree.parent[target] == 0) {
    return {};
  }

  const std::size_t max_steps = tree.parent.size();
  std::vector<vertex_id> path;
  for (vertex_id v = target; v != tree.source; v = tree.parent[v]) {
    if (v == 0 || path.size() == max_steps) {
      return {};
    }
    path.push_back(v);
  }
  path.push_back(tree.source);
  std::ranges::reverse(path);
  return path;
}
} // namespace gl::algorithms
