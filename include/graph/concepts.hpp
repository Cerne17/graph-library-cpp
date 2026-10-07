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
concept graph_like = requires(const G& g, vertex_id u) {
  { G::direction_type::is_directed } -> std::convertible_to<bool>;
  { G::weighting_type::is_weighted } -> std::convertible_to<bool>;
};
}
