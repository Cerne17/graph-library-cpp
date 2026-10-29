#pragma once

#include "graph/concepts.hpp"
#include "graph/types.hpp"

#include <algorithm>
#include <cassert>
#include <vector>

namespace gl::algorithms {
struct degree_stats
{
  vertex_id vertex_count = 0;
  edge_index edge_count = 0; // undirected: each edge once; directed: arcs
  edge_index min_degree = 0;
  edge_index max_degree = 0;
  double average_degree = 0.0; // total degree / n
  double median_degree = 0.0;
};

// Degree statistics, counting each vertex's degree with for_each_neighbor
// (out-degree on directed graphs). O(n log n + m).
// Precondition: at least one vertex.
template<graph_like G>
degree_stats compute_degree_stats(const G& g)
{
  const vertex_id n = g.vertex_count();
  assert(n >= 1 && "compute_degree_stats: graph has no vertices");

  std::vector<edge_index> degrees;
  degrees.reserve(n);
  edge_index total = 0;
  for (vertex_id u = 1; u <= n; ++u) {
    edge_index degree = 0;
    g.for_each_neighbor(u, [&](vertex_id) { ++degree; });
    degrees.push_back(degree);
    total += degree;
  }
  std::ranges::sort(degrees);

  degree_stats stats;
  stats.vertex_count = n;
  stats.edge_count = G::direction_type::is_directed ? total : total / 2;
  stats.min_degree = degrees.front();
  stats.max_degree = degrees.back();
  stats.average_degree = static_cast<double>(total) / n;
  stats.median_degree =
    (n % 2 == 1)
      ? static_cast<double>(degrees[n / 2])
      : static_cast<double>(degrees[n / 2 - 1] + degrees[n / 2]) / 2.0;
  return stats;
}
} // namespace gl::algorithms
