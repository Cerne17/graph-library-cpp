#pragma once

#include "graph/concepts.hpp"
#include "graph/types.hpp"

#include <cassert>
#include <cstdint>
#include <queue>
#include <stack>
#include <utility>
#include <vector>

namespace gl::algorithms {
// Result of a traversal rooted at one vertex. Both vectors are sized n + 1;
// index 0 is unused.
struct search_tree
{
  vertex_id source = 0;
  std::vector<vertex_id> parent;   // parent[v], 0 for the source and unreached
  std::vector<std::int32_t> level; // level[v], -1 if unreached
};

// Breadth-first search. level[v] is the distance from source to v.
// Marks a vertex visited when it is enqueued, and visits neighbors in the
// order for_each_neighbor yields them (ascending on CSR and matrix).
template<graph_like G>
search_tree bfs(const G& g, vertex_id source)
{
  const vertex_id n = g.vertex_count();
  assert(source >= 1 && source <= n && "bfs: source out of range");

  search_tree tree{ source,
                    std::vector<vertex_id>(n + 1, 0),
                    std::vector<std::int32_t>(n + 1, -1) };
  std::vector<bool> visited(n + 1, false);
  std::queue<vertex_id> queue;

  visited[source] = true;
  tree.level[source] = 0;
  queue.push(source);

  while (!queue.empty()) {
    const vertex_id u = queue.front();
    queue.pop();

    g.for_each_neighbor(u, [&](vertex_id v) {
      if (!visited[v]) {
        visited[v] = true;
        tree.parent[v] = u;
        tree.level[v] = tree.level[u] + 1;
        queue.push(v);
      }
    });
  }
  return tree;
}

// Depth-first search, iterative, reproducing the recursive DFS tree: a stack
// of (vertex, parent) pairs, a vertex is settled when popped, and neighbors
// are pushed in reverse so they are visited ascending.
// level[v] is v's depth in the DFS tree, not its distance from source.
template<graph_like G>
search_tree dfs(const G& g, vertex_id source)
{
  const vertex_id n = g.vertex_count();
  assert(source >= 1 && source <= n && "dfs: source out of range");

  search_tree tree{ source,
                    std::vector<vertex_id>(n + 1, 0),
                    std::vector<std::int32_t>(n + 1, -1) };
  std::vector<bool> visited(n + 1, false);
  std::stack<std::pair<vertex_id, vertex_id>> stack; // (vertex, parent)
  std::vector<vertex_id> buffer; // reused for every vertex's neighbors

  stack.push({ source, 0 });

  while (!stack.empty()) {
    const auto [u, p] = stack.top();
    stack.pop();

    if (visited[u]) {
      continue;
    }
    visited[u] = true;
    tree.parent[u] = p;
    tree.level[u] = (u == source) ? 0 : tree.level[p] + 1;

    buffer.clear();
    g.for_each_neighbor(u, [&](vertex_id v) {
      if (!visited[v]) {
        buffer.push_back(v);
      }
    });
    for (auto it = buffer.rbegin(); it != buffer.rend(); ++it) {
      stack.push({ *it, u });
    }
  }
  return tree;
}
} // namespace gl::algorithms
