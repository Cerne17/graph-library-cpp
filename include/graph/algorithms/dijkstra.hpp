#pragma once

#include "graph/concepts.hpp"
#include "graph/types.hpp"

#include <vector>

namespace gl::algorithms {
template<std::floating_point W>
struct shortest_paths
{
  vertex_id source = 0;
  std::vector<W> dist;
  std::vector<vertex_id> parent;
};

template<weighted_graph G>
shortest_paths<typename G::weight_type> dijkstra_array(const G& g,
                                                       vertex_id source);
} // namespace gl::algorithms
