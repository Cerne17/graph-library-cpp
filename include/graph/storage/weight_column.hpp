#pragma once

#include "graph/policies.hpp"

#include <vector>

namespace gl {
// General version, used by default: stores nothing
template<weight_policy W, bool = W::is_weighted>
struct weight_column
{};

// Specialization, chosen when the second argument is true.
// Holds the weights
template<weight_policy W>
struct weight_column<W, true>
{
  std::vector<typename W::weight_type> values;
  bool has_negative = false;
};

template<weight_policy W, bool = W::is_weighted>
struct weight_type_member
{};

template<weight_policy W>
struct weight_type_member<W, true>
{
  using weight_type = typename W::weight_type;
};
} // namespace gl
