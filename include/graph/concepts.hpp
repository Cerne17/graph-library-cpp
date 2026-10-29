#pragma once

#include <concepts>

#include "graph/policies.hpp"
#include "graph/types.hpp"

namespace gl {

struct neighbor_probe
{
  void operator()(vertex_id) const;
};

template<typename W>
struct edge_probe
{
  void operator()(vertex_id, W) const;
};

template<typename G>
concept graph_like =
  direction_policy<typename G::direction_type> &&
  weight_policy<typename G::weighting_type> &&
  requires(const G& g, vertex_id u) {
    { g.vertex_count() } -> std::convertible_to<vertex_id>;
    g.for_each_neighbor(u, neighbor_probe{}); // this must compile
  };

template<typename G>
concept weighted_graph =
  graph_like<G> && G::weighting_type::is_weighted &&
  std::same_as<typename G::weight_type,
               typename G::weighting_type::weight_type> &&
  requires(const G& g, vertex_id u) {
    g.for_each_edge(u, edge_probe<typename G::weight_type>{});
  };
} // namespace gl
