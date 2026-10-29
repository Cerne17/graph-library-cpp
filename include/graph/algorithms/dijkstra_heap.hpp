#pragma once

#include "graph/algorithms/shortest_paths.hpp"
#include "graph/concepts.hpp"
#include "graph/heap.hpp"
#include "graph/types.hpp"

namespace gl::algorithms {
// Dijkstra with gl::indexed_min_heap: each vertex enters the heap at most
// once, and a relaxation that improves dist[v] calls update_key(v, ...) on it.
// O((n + m) log n) time, O(n) extra space. Best on sparse graphs.
// Throws std::invalid_argument if g has a negative weight.
template<weighted_graph G>
shortest_paths<typename G::weight_type> dijkstra_heap(const G& g,
                                                      vertex_id source);
} // namespace gl::algorithms
