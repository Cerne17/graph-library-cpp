#include "doctest/doctest.h"
#include "graph/policies.hpp"
#include "graph/storage/weight_column.hpp"

#include <concepts>
#include <type_traits>
#include <vector>

static_assert(std::is_empty_v<gl::weight_column<gl::unweighted>>);
static_assert(!std::is_empty_v<gl::weight_column<gl::weighted<>>>);
static_assert(!std::is_empty_v<gl::weight_column<gl::weighted<float>>>);
static_assert(
  std::same_as<decltype(gl::weight_column<gl::weighted<float>>::values),
               std::vector<float>>);
