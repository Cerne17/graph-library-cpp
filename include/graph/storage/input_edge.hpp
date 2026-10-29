#pragma once

#include "graph/policies.hpp"
#include "graph/types.hpp"

namespace gl {
template<weight_policy W, bool = W::is_weighted>
struct input_edge
{
  using type = edge;
};

template<weight_policy W>
struct input_edge<W, true>
{
  using type = weighted_edge<typename W::weight_type>;
};

template<weight_policy W>
using input_edge_t = typename input_edge<W>::type;
} // namespace gl
