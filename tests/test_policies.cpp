#include "doctest/doctest.h"
#include "graph/policies.hpp"

#include <concepts>

static_assert(DirectionPolicy<Undirected>);                   // true
static_assert(DirectionPolicy<Directed>);                     // true
static_assert(!DirectionPolicy<int>);                         // true
static_assert(Directed::directed);                            // true
static_assert(WeightPolicy<Unweighted>);                      // true
static_assert(WeightPolicy<Weighted<>>);                      // true
static_assert(WeightPolicy<Weighted<float>>);                 // true
static_assert(std::same_as<Weighted<>::weight_type, double>); // true

struct Liar
{
  static constexpr bool weighted = true;
};
static_assert(!WeightPolicy<Liar>);

TEST_CASE("weight policy flags are readable at runtime")
{
  CHECK(Weighted<>::weighted);
  CHECK_FALSE(Unweighted::weighted);
}
