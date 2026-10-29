#pragma once

#include "graph/concepts.hpp"
#include "graph/io/edge_list.hpp"

#include <filesystem>

namespace gl::io {
// Reads an edge-list file and builds a G from it.
// Throws whatever read_edge_list throws; adds no checks of its own.
template<graph_like G>
G load_graph(const std::filesystem::path& path)
{
  const auto list = read_edge_list<typename G::weighting_type>(path);
  return G::from_edges(list.n, list.edges);
}
} // namespace gl::io
