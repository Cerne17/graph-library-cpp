#include "doctest/doctest.h"
#include "graph/policies.hpp"
#include "graph/storage/input_edge.hpp"
#include "graph/types.hpp"

#include <concepts>
#include <type_traits>

static_assert(std::same_as<gl::input_edge_t<gl::unweighted>, gl::edge>);
static_assert(
  std::same_as<gl::input_edge_t<gl::weighted<>>, gl::weighted_edge<double>>);
static_assert(std::same_as<gl::input_edge_t<gl::weighted<float>>,
                           gl::weighted_edge<float>>);
