#pragma once

#include "graph/types.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <vector>

namespace gl::algorithms {
// Anything rooted at a source with a parent array: search_tree (BFS/DFS) and
// shortest_paths (Dijkstra) both qualify.
template<typename T>
concept parent_tree = requires(const T& t) {
  { t.source } -> std::convertible_to<vertex_id>;
  { t.parent } -> std::convertible_to<const std::vector<vertex_id>&>;
};

// Vertices on the tree path from tree.source to target, source first and
// target last. {source} when target == source; empty when target is
// unreachable (parent[target] == 0 and target != source).
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

  std::vector<vertex_id> path;
  for (vertex_id v = target; v != tree.source; v = tree.parent[v]) {
    path.push_back(v);
  }
  path.push_back(tree.source);
  std::ranges::reverse(path);
  return path;
}
} // namespace gl::algorithms
