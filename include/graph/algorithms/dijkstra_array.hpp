#pragma once

#include "graph/algorithms/shortest_paths.hpp"
#include "graph/concepts.hpp"
#include "graph/types.hpp"

namespace gl::algorithms {
// Dijkstra with a plain array: each round scans all unvisited vertices for the
// one with the smallest dist, then relaxes its edges. Stops early when the
// smallest dist left is +inf (the rest is unreachable).
// O(n^2 + m) time, O(n) extra space. Best on dense graphs (m close to n^2).
// Throws std::invalid_argument if g has a negative weight.
template<weighted_graph G>
shortest_paths<typename G::weight_type> dijkstra_array(const G& g,
                                                       vertex_id source);
} // namespace gl::algorithms
