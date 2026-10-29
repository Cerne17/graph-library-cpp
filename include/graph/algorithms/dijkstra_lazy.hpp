#pragma once

#include "graph/algorithms/shortest_paths.hpp"
#include "graph/concepts.hpp"
#include "graph/types.hpp"

namespace gl::algorithms {
// Dijkstra with std::priority_queue and lazy deletion: a relaxation pushes a
// new (dist, vertex) entry instead of updating the old one, and a popped entry
// whose dist is larger than dist[vertex] is stale and skipped.
// O((n + m) log m) time, O(m) extra space (the stale entries).
// The baseline that shows what the indexed heap buys.
// Throws std::invalid_argument if g has a negative weight.
template<weighted_graph G>
shortest_paths<typename G::weight_type> dijkstra_lazy(const G& g,
                                                      vertex_id source);
} // namespace gl::algorithms
