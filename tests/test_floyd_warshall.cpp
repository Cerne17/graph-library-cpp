// Floyd-Warshall on hand-checked graphs. Expected matrices are worked out by
// hand from the arcs written next to them.

#include "graph/algorithms/floyd_warshall.hpp"

#include "graph/algorithms/bellman_ford.hpp"
#include "graph/algorithms/path.hpp"
#include "graph/graph.hpp"
#include "graph/io/edge_list.hpp"
#include "graph/io/fixed_point.hpp"
#include "graph/policies.hpp"
#include "graph/types.hpp"

#include "doctest/doctest.h"

#include <limits>
#include <vector>

namespace {
using gl::algorithms::all_pairs;
using gl::algorithms::floyd_warshall;
using gl::algorithms::reconstruct_path;

using wedge = gl::weighted_edge<double>;
using ids = std::vector<gl::vertex_id>;
using directed_csr = gl::csr_graph<gl::directed, gl::weighted<>>;
using directed_matrix = gl::matrix_graph<gl::directed, gl::weighted<>>;

constexpr double inf = std::numeric_limits<double>::infinity();

template<typename W>
concept all_pairs_type = requires { typename all_pairs<W>; };

template<typename A>
concept tree_from_rvalue = requires { std::declval<A>().tree_from(1); };
} // namespace

static_assert(gl::algorithms::parent_tree<all_pairs<double>::row_tree>);
static_assert(all_pairs_type<double>);
static_assert(!all_pairs_type<int>);
static_assert(!tree_from_rvalue<all_pairs<double>>);
static_assert(tree_from_rvalue<const all_pairs<double>&>);

TEST_CASE_TEMPLATE("floyd_warshall: hand-checked graph", G, directed_csr, directed_matrix)
{
  // 1->2 (3), 2->3 (-2), 1->3 (4), 3->1 (1), vertex 4 isolated
  // 1->3 via 2 = 1; 2->1 via 3 = -1; 3->2 via 1 = 4
  const std::vector<wedge> edges{ { 1, 2, 3.0 },
                                  { 2, 3, -2.0 },
                                  { 1, 3, 4.0 },
                                  { 3, 1, 1.0 } };
  const auto r = floyd_warshall(G::from_edges(4, edges));

  const std::vector<std::vector<double>> expected{
    {}, { 0.0, 3.0, 1.0, inf }, { -1.0, 0.0, -2.0, inf }, { 1.0, 4.0, 0.0, inf },
    { inf, inf, inf, 0.0 }
  };
  REQUIRE(r.vertex_count() == 4);
  for (gl::vertex_id i = 1; i <= 4; ++i) {
    for (gl::vertex_id j = 1; j <= 4; ++j) {
      CAPTURE(i);
      CAPTURE(j);
      CHECK(r.dist(i, j) == expected[i][j - 1]);
      CHECK_FALSE(r.minus_infinity(i, j));
    }
  }
  CHECK_FALSE(r.negative_cycle());
  CHECK(reconstruct_path(r.tree_from(3), 2) == ids{ 3, 1, 2 });
  CHECK(reconstruct_path(r.tree_from(1), 3) == ids{ 1, 2, 3 });
  CHECK(reconstruct_path(r.tree_from(2), 1) == ids{ 2, 3, 1 });
  CHECK(reconstruct_path(r.tree_from(4), 1).empty());
  CHECK(r.bytes() == 5 * 5 * (sizeof(double) + sizeof(gl::vertex_id)));
}

TEST_CASE("floyd_warshall: a negative cycle marks only the pairs through it")
{
  // 1->2 (1), 2->3 (-3), 3->2 (1), 3->4 (1), 5->4 (2): the cycle 2->3->2
  // (-2) lies on every walk 1 -> 4, 2 -> 4, 3 -> 4, but not on 5 -> 4
  const std::vector<wedge> edges{ { 1, 2, 1.0 }, { 2, 3, -3.0 }, { 3, 2, 1.0 },
                                  { 3, 4, 1.0 }, { 5, 4, 2.0 } };
  const auto r = floyd_warshall(directed_csr::from_edges(5, edges));

  CHECK(r.negative_cycle());
  CHECK(r.minus_infinity(1, 4));
  CHECK(r.minus_infinity(2, 4));
  CHECK(r.minus_infinity(3, 4));
  CHECK(r.minus_infinity(2, 2));
  CHECK_FALSE(r.minus_infinity(5, 4));
  CHECK_FALSE(r.minus_infinity(4, 1)); // 4 reaches nothing
  CHECK(r.dist(5, 4) == 2.0);
  CHECK(reconstruct_path(r.tree_from(5), 4) == ids{ 5, 4 });
  for (gl::vertex_id i = 1; i <= 5; ++i) {
    CHECK(r.tree_from(i).parent[i] == 0); // the diagonal of pred is reset
  }
}

TEST_CASE("floyd_warshall: a negative self-loop passed in")
{
  // 1->2 (1), 2->3 (1), plus the loop 3 3 (-1)
  const std::vector<wedge> edges{ { 1, 2, 1.0 }, { 2, 3, 1.0 } };
  const std::vector<wedge> loops{ { 3, 3, -1.0 } };
  const auto g = directed_csr::from_edges(3, edges);

  CHECK_FALSE(floyd_warshall(g).negative_cycle());
  const auto r = floyd_warshall(g, loops);
  CHECK(r.negative_cycle());
  CHECK(r.minus_infinity(3, 3));
  CHECK(r.minus_infinity(1, 3));
  CHECK_FALSE(r.minus_infinity(3, 1)); // 3 reaches nothing else
  CHECK_FALSE(r.minus_infinity(1, 2)); // 1 -> 2 does not pass through 3
  CHECK(r.dist(1, 2) == 1.0);
}

TEST_CASE("floyd_warshall: parallel CSR arcs keep the lightest")
{
  const std::vector<wedge> edges{ { 1, 2, 5.0 }, { 1, 2, 2.0 } };
  const auto r = floyd_warshall(directed_csr::from_edges(2, edges));
  CHECK(r.dist(1, 2) == 2.0);
  CHECK(r.dist(2, 1) == inf);
}

TEST_CASE("floyd_warshall: the pivot observer")
{
  // the hand-checked graph above: at k = 4 no row reaches 4, so all skip
  const std::vector<wedge> edges{ { 1, 2, 3.0 },
                                  { 2, 3, -2.0 },
                                  { 1, 3, 4.0 },
                                  { 3, 1, 1.0 } };
  std::vector<gl::vertex_id> pivots;
  std::vector<gl::vertex_id> rows;
  const auto r = floyd_warshall(directed_csr::from_edges(4, edges),
                                {},
                                [&](gl::vertex_id k, gl::vertex_id done) {
                                  pivots.push_back(k);
                                  rows.push_back(done);
                                });
  CHECK(pivots == ids{ 1, 2, 3, 4 });
  // rows i != k with a path i -> k so far: k = 1: row 3 (2 -> 1 only
  // appears at k = 3); k = 2: rows 1 and 3; k = 3: rows 1 and 2; k = 4: none
  CHECK(rows == ids{ 1, 2, 2, 0 });
  CHECK(r.dist(1, 3) == 1.0);
}

TEST_CASE("floyd_warshall: 2-decimal weights scaled to integers stay exact")
{
  // the cycle 1 -> 2 -> 3 -> 1 weighs 0.3 - 0.1 - 0.2 = 0
  using reader_policy = gl::weighted<double>;

  // control: unscaled doubles round it below 0, a false negative cycle
  REQUIRE((0.3 + -0.1) + -0.2 < 0.0);
  const auto unscaled_list =
    gl::io::read_edge_list<reader_policy>("tests/data/zero_cycle_decimals.txt");
  const auto unscaled = directed_csr::from_edges(unscaled_list.n, unscaled_list.edges);
  CHECK(floyd_warshall(unscaled).negative_cycle());

  // scaled by 100 from the text: 30, -10, -20, sums exact
  const auto list = gl::io::read_edge_list<reader_policy>(
    "tests/data/zero_cycle_decimals.txt", gl::io::fixed_point_weight<double>{ 2 });
  const auto g = directed_csr::from_edges(list.n, list.edges);
  const auto r = floyd_warshall(g);
  CHECK_FALSE(r.negative_cycle());
  CHECK_FALSE(gl::algorithms::find_negative_cycle(g).found);
  const auto to_1 = gl::algorithms::bellman_ford(g, 1);
  CHECK(to_1.dist[2] == -30.0); // 2 -> 3 -> 1 = -0.10 - 0.20
  CHECK(r.dist(2, 1) == to_1.dist[2]);
}
