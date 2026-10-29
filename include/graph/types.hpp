#pragma once

#include <concepts>
#include <cstdint>

namespace gl {
using vertex_id = std::uint32_t;
using edge_index = std::uint64_t;

struct edge
{
  vertex_id u;
  vertex_id v;
};

template<std::floating_point W>
struct weighted_edge
{
  vertex_id u;
  vertex_id v;
  W weight;
};
} // namespace gl
