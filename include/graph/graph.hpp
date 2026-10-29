#pragma once

#include "graph/policies.hpp"
#include "graph/storage/csr.hpp"
#include "graph/storage/matrix.hpp"

namespace gl {
// User-facing names for the two representations.
template<direction_policy D, weight_policy W>
using csr_graph = csr_storage<D, W>;

template<direction_policy D, weight_policy W>
using matrix_graph = matrix_storage<D, W>;
} // namespace gl
