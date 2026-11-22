// fixed_point_weight: decimal text to an exact integer-valued weight.
// Expected values are the decimal tokens times 10^decimals, by hand.

#include "graph/io/fixed_point.hpp"

#include "graph/io/edge_list.hpp"
#include "graph/policies.hpp"

#include "doctest/doctest.h"

#include <stdexcept>

namespace {
using gl::io::fixed_point_weight;

template<typename T>
concept fixed_point_type = requires { typename fixed_point_weight<T>; };
} // namespace

static_assert(fixed_point_type<double>);
static_assert(fixed_point_type<float>);
static_assert(!fixed_point_type<int>);

TEST_CASE("fixed_point_weight: two decimals")
{
  const fixed_point_weight<double> parse{ 2 };

  CHECK(parse("1.19") == 119.0);
  CHECK(parse("-5.00") == -500.0);
  CHECK(parse("10") == 1000.0);
  CHECK(parse("0.3") == 30.0);
  CHECK(parse("-0.1") == -10.0);
  CHECK(parse("+2.5") == 250.0);
  CHECK(parse("0") == 0.0);
  CHECK(parse.scale() == 100.0);
}

TEST_CASE("fixed_point_weight: rejected tokens")
{
  const fixed_point_weight<double> parse{ 2 };

  CHECK_THROWS_WITH_AS(parse("1.234"),
                       doctest::Contains("more than 2 decimals: 1.234"),
                       std::invalid_argument);
  for (const char* token : { "1e2", ".", "-", "1.", "abc", "1.2x", "" }) {
    CAPTURE(token);
    CHECK_THROWS_WITH_AS(parse(token),
                         doctest::Contains("not a decimal weight"),
                         std::invalid_argument);
  }
  CHECK_THROWS_WITH_AS(parse("1234567890123456789012345"),
                       doctest::Contains("too large to hold exactly"),
                       std::invalid_argument);
}

TEST_CASE("fixed_point_weight: no decimals, at the exactness limit")
{
  const fixed_point_weight<double> parse{ 0 };

  CHECK(parse("7") == 7.0);
  CHECK(parse.scale() == 1.0);
  CHECK_THROWS_AS(parse("7.5"), std::invalid_argument);

  // 2^53 is the largest integer up to which double holds every integer
  CHECK(parse("9007199254740992") == 9007199254740992.0);
  CHECK_THROWS_WITH_AS(parse("9007199254740993"),
                       doctest::Contains("too large to hold exactly"),
                       std::invalid_argument);

  // 2^24 for float
  const fixed_point_weight<float> parse_float{ 0 };
  CHECK(parse_float("16777216") == 16777216.0f);
  CHECK_THROWS_AS(parse_float("16777217"), std::invalid_argument);
}

TEST_CASE("fixed_point_weight: through read_edge_list")
{
  const auto g = gl::io::read_edge_list<gl::weighted<double>>(
    "tests/data/weighted_small.txt", fixed_point_weight<double>{ 2 });
  REQUIRE(g.edges.size() == 2);
  CHECK(g.edges[0].weight == 50.0); // 0.5
  CHECK(g.edges[1].weight == 150.0); // 1.5

  CHECK_THROWS_WITH_AS(
    gl::io::read_edge_list<gl::weighted<double>>(
      "tests/data/three_decimals.txt", fixed_point_weight<double>{ 2 }),
    doctest::Contains("three_decimals.txt:2: weight has more than 2 decimals"),
    std::runtime_error);
}
