#pragma once

#include <cstdint>

namespace gl {
using vertex_id = std::uint32_t;
using edge_index = std::uint64_t;

struct edge
{
  vertex_id u;
  vertex_id v;
};
} // namespace gl
