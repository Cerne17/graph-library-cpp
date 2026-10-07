#include "doctest/doctest.h"
#include "graph/policies.hpp"

#include <concepts>

static_assert(gl::direction_policy<gl::undirected>);              // true
static_assert(gl::direction_policy<gl::directed>);                // true
static_assert(!gl::direction_policy<int>);                        // true
static_assert(gl::directed::is_directed);                         // true
static_assert(gl::weight_policy<gl::unweighted>);                 // true
static_assert(gl::weight_policy<gl::weighted<>>);                 // true
static_assert(gl::weight_policy<gl::weighted<float>>);            // true
static_assert(std::same_as<gl::weighted<>::weight_type, double>); // true

struct Liar
{
  static constexpr bool is_weighted = true;
};

static_assert(!gl::weight_policy<Liar>);

TEST_CASE("weight policy flags are readable at runtime")
{
  CHECK(gl::weighted<>::is_weighted);
  CHECK_FALSE(gl::unweighted::is_weighted);
}
