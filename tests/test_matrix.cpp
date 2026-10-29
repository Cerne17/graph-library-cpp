#include "doctest/doctest.h"
#include "graph/concepts.hpp"
#include "graph/policies.hpp"
#include "graph/storage/csr.hpp"
#include "graph/storage/matrix.hpp"
#include "graph/types.hpp"

#include <concepts>
#include <cstddef>
#include <random>
#include <utility>
#include <vector>

using undirected_unweighted_matrix =
  gl::matrix_storage<gl::undirected, gl::unweighted>;
using directed_unweighted_matrix =
  gl::matrix_storage<gl::directed, gl::unweighted>;
using undirected_weighted_matrix =
  gl::matrix_storage<gl::undirected, gl::weighted<>>;
using directed_weighted_matrix =
  gl::matrix_storage<gl::directed, gl::weighted<>>;

static_assert(gl::graph_like<undirected_unweighted_matrix>);
static_assert(gl::graph_like<directed_unweighted_matrix>);
static_assert(!gl::weighted_graph<undirected_unweighted_matrix>);
static_assert(!gl::weighted_graph<directed_unweighted_matrix>);
static_assert(gl::weighted_graph<undirected_weighted_matrix>);
static_assert(gl::weighted_graph<directed_weighted_matrix>);

// policy aliases are forwarded unchanged
static_assert(
  std::same_as<undirected_unweighted_matrix::direction_type, gl::undirected>);
static_assert(
  std::same_as<directed_weighted_matrix::direction_type, gl::directed>);
static_assert(
  std::same_as<undirected_unweighted_matrix::weighting_type, gl::unweighted>);
static_assert(
  std::same_as<undirected_weighted_matrix::weighting_type, gl::weighted<>>);

// G::weight_type exists only on weighted graphs
static_assert(std::same_as<undirected_weighted_matrix::weight_type, double>);
static_assert(
  std::same_as<gl::matrix_storage<gl::directed, gl::weighted<float>>::weight_type,
               float>);
static_assert(!gl::has_weight_type<undirected_unweighted_matrix>);

// input_edge picks the edge struct that matches the weighting
static_assert(
  std::same_as<undirected_unweighted_matrix::input_edge, gl::edge>);
static_assert(std::same_as<directed_weighted_matrix::input_edge,
                           gl::weighted_edge<double>>);

namespace {
template<typename G>
concept has_negative_weight_query =
  requires(const G& g) { g.has_negative_weight(); };

template<typename G>
concept has_edge_visitor = requires(const G& g) {
  g.for_each_edge(gl::vertex_id{ 1 }, [](gl::vertex_id, double) {});
};
} // namespace

// weight-only members are rejected on unweighted graphs
static_assert(has_negative_weight_query<undirected_weighted_matrix>);
static_assert(!has_negative_weight_query<undirected_unweighted_matrix>);
static_assert(has_edge_visitor<directed_weighted_matrix>);
static_assert(!has_edge_visitor<directed_unweighted_matrix>);

namespace {
const std::vector<gl::edge> example_edges{ { 1, 2 },
                                           { 2, 5 },
                                           { 5, 3 },
                                           { 4, 5 },
                                           { 1, 5 } };
const std::vector<gl::weighted_edge<double>> example_weighted_edges{
  { 1, 2, 1.2 }, { 2, 5, 2.5 }, { 5, 3, 5.3 }, { 4, 5, 4.5 }, { 1, 5, 1.5 }
};
const std::vector<gl::weighted_edge<double>> example_directed_weighted_edges{
  { 1, 2, 1.2 }, { 2, 5, 2.5 }, { 5, 3, 5.3 },
  { 4, 5, 4.5 }, { 1, 5, 1.5 }, { 5, 1, 5.1 }
};

template<typename G>
std::vector<gl::vertex_id> neighbors_of(const G& g, gl::vertex_id u)
{
  std::vector<gl::vertex_id> out;
  g.for_each_neighbor(u, [&](gl::vertex_id v) { out.push_back(v); });
  return out;
}

template<typename G>
std::vector<std::pair<gl::vertex_id, double>> edges_of(const G& g,
                                                       gl::vertex_id u)
{
  std::vector<std::pair<gl::vertex_id, double>> out;
  g.for_each_edge(u, [&](gl::vertex_id v, double w) { out.emplace_back(v, w); });
  return out;
}
} // namespace

// ---- unweighted -----------------------------------------------------------

TEST_CASE("matrix: undirected unweighted example graph")
{
  const auto s = undirected_unweighted_matrix::from_edges(5, example_edges);

  CHECK(s.vertex_count() == 5);
  CHECK(neighbors_of(s, 5) == std::vector<gl::vertex_id>{ 1, 2, 3, 4 });
  CHECK(neighbors_of(s, 1) == std::vector<gl::vertex_id>{ 2, 5 });
  CHECK(neighbors_of(s, 3) == std::vector<gl::vertex_id>{ 5 });
}

TEST_CASE("matrix: directed unweighted example graph")
{
  const auto t = directed_unweighted_matrix::from_edges(5, example_edges);

  CHECK(t.vertex_count() == 5);
  CHECK(neighbors_of(t, 5) == std::vector<gl::vertex_id>{ 3 });
  CHECK(neighbors_of(t, 1) == std::vector<gl::vertex_id>{ 2, 5 });
  CHECK(neighbors_of(t, 3).empty()); // sink
}

TEST_CASE("matrix: has_edge")
{
  const auto s = undirected_unweighted_matrix::from_edges(5, example_edges);
  const auto t = directed_unweighted_matrix::from_edges(5, example_edges);

  CHECK(s.has_edge(1, 5));
  CHECK(s.has_edge(5, 1));
  CHECK_FALSE(s.has_edge(3, 4));

  CHECK(t.has_edge(1, 2));
  CHECK_FALSE(t.has_edge(2, 1));
  CHECK_FALSE(t.has_edge(3, 4));
}

TEST_CASE("matrix: isolated vertex")
{
  const auto s = undirected_unweighted_matrix::from_edges(6, example_edges);

  CHECK(s.vertex_count() == 6);
  CHECK(neighbors_of(s, 6).empty());
  CHECK(neighbors_of(s, 5) == std::vector<gl::vertex_id>{ 1, 2, 3, 4 });
}

TEST_CASE("matrix: graph without edges")
{
  const std::vector<gl::edge> no_edges;
  const auto s = undirected_unweighted_matrix::from_edges(4, no_edges);
  const auto t = directed_unweighted_matrix::from_edges(4, no_edges);

  for (gl::vertex_id u = 1; u <= 4; ++u) {
    CHECK(neighbors_of(s, u).empty());
    CHECK(neighbors_of(t, u).empty());
  }
}

TEST_CASE("matrix: neighbors across 64-bit word boundaries")
{
  // n = 130 gives 3 words per row: bits 0-63, 64-127, 128-130
  const std::vector<gl::edge> edges{
    { 1, 63 }, { 1, 64 }, { 1, 65 }, { 1, 128 }, { 1, 130 }
  };
  const auto s = undirected_unweighted_matrix::from_edges(130, edges);

  CHECK(neighbors_of(s, 1) ==
        std::vector<gl::vertex_id>{ 63, 64, 65, 128, 130 });
  CHECK(neighbors_of(s, 64) == std::vector<gl::vertex_id>{ 1 });
  CHECK(neighbors_of(s, 130) == std::vector<gl::vertex_id>{ 1 });
  CHECK(neighbors_of(s, 127).empty());
  CHECK(s.has_edge(1, 128));
  CHECK(s.has_edge(130, 1));
  CHECK_FALSE(s.has_edge(1, 127));
  CHECK_FALSE(s.has_edge(1, 129));
}

TEST_CASE("matrix: n a multiple of 64 still has room for bit n")
{
  // bits 0..128 need 3 words, not 128 / 64 = 2
  const std::vector<gl::edge> edges{ { 1, 128 }, { 127, 128 } };
  const auto s = undirected_unweighted_matrix::from_edges(128, edges);

  CHECK(neighbors_of(s, 1) == std::vector<gl::vertex_id>{ 128 });
  CHECK(neighbors_of(s, 128) == std::vector<gl::vertex_id>{ 1, 127 });
  CHECK(neighbors_of(s, 2).empty());
}

TEST_CASE("matrix: bitset memory")
{
  const gl::detail::matrix_cells<gl::unweighted> cells(130);
  const gl::detail::matrix_cells<gl::unweighted> boundary_cells(128);

  CHECK(cells.bytes() == 131 * 3 * 8);          // 131 rows of 3 words
  CHECK(boundary_cells.bytes() == 129 * 3 * 8); // 129 rows of 3 words
}

// ---- weighted -------------------------------------------------------------

TEST_CASE("matrix: undirected weighted example graph")
{
  const auto s =
    undirected_weighted_matrix::from_edges(5, example_weighted_edges);

  const std::vector<std::pair<gl::vertex_id, double>> expected_5{
    { 1, 1.5 }, { 2, 2.5 }, { 3, 5.3 }, { 4, 4.5 }
  };

  CHECK(s.vertex_count() == 5);
  CHECK(edges_of(s, 5) == expected_5);
  CHECK(neighbors_of(s, 5) == std::vector<gl::vertex_id>{ 1, 2, 3, 4 });
  CHECK(s.has_edge(5, 1));
  CHECK_FALSE(s.has_edge(3, 4));
}

TEST_CASE("matrix: directed weighted example graph")
{
  const auto t =
    directed_weighted_matrix::from_edges(5, example_directed_weighted_edges);

  const std::vector<std::pair<gl::vertex_id, double>> expected_5{
    { 1, 5.1 }, { 3, 5.3 }
  };

  CHECK(edges_of(t, 5) == expected_5);
  CHECK(edges_of(t, 3).empty());
  CHECK(t.has_edge(1, 2));
  CHECK_FALSE(t.has_edge(2, 1));
}

TEST_CASE("matrix: parallel edges keep the minimum weight")
{
  const std::vector<gl::weighted_edge<double>> parallel_edges{ { 1, 2, 3.0 },
                                                               { 1, 2, 1.0 },
                                                               { 2, 1, 2.0 } };
  const auto s = undirected_weighted_matrix::from_edges(2, parallel_edges);

  const std::vector<std::pair<gl::vertex_id, double>> expected_1{ { 2, 1.0 } };
  const std::vector<std::pair<gl::vertex_id, double>> expected_2{ { 1, 1.0 } };

  CHECK(edges_of(s, 1) == expected_1);
  CHECK(edges_of(s, 2) == expected_2);
}

TEST_CASE("matrix: has_negative_weight")
{
  const std::vector<gl::weighted_edge<double>> negative_edges{
    { 1, 2, -1.0 }, { 2, 3, 2.0 }
  };

  const auto s =
    undirected_weighted_matrix::from_edges(5, example_weighted_edges);
  const auto n = directed_weighted_matrix::from_edges(3, negative_edges);

  CHECK_FALSE(s.has_negative_weight());
  CHECK(n.has_negative_weight());
}

TEST_CASE("matrix: dense weighted memory")
{
  const gl::detail::matrix_cells<gl::weighted<>> cells(5);
  const gl::detail::matrix_cells<gl::weighted<float>> float_cells(5);

  CHECK(cells.bytes() == 6 * 6 * 8);       // (n + 1)^2 doubles
  CHECK(float_cells.bytes() == 6 * 6 * 4); // (n + 1)^2 floats
  CHECK_FALSE(cells.has_negative);
}

// ---- differential: matrix against CSR -------------------------------------

namespace {
// Random graph with distinct pairs: for undirected graphs, (u, v) and (v, u)
// count as the same pair.
template<bool directed>
std::vector<gl::weighted_edge<double>> random_edges(std::mt19937& rng,
                                                    gl::vertex_id n)
{
  std::vector<bool> used((n + 1) * (n + 1), false);
  std::vector<gl::weighted_edge<double>> edges;
  const auto attempts = 3 * n;
  for (gl::vertex_id i = 0; i < attempts; ++i) {
    const gl::vertex_id u = 1 + rng() % n;
    const gl::vertex_id v = 1 + rng() % n;
    if (u == v || used[u * (n + 1) + v]) {
      continue;
    }
    used[u * (n + 1) + v] = true;
    if (!directed) {
      used[v * (n + 1) + u] = true;
    }
    // weights in [-10, 10], quarter steps: exactly representable
    const double w = static_cast<double>(rng() % 81) / 4.0 - 10.0;
    edges.push_back({ u, v, w });
  }
  return edges;
}

std::vector<gl::edge> drop_weights(
  const std::vector<gl::weighted_edge<double>>& edges)
{
  std::vector<gl::edge> out;
  for (const auto& e : edges) {
    out.push_back({ e.u, e.v });
  }
  return out;
}

template<gl::direction_policy D>
void check_matrix_matches_csr(std::mt19937& rng)
{
  constexpr bool directed = D::is_directed;

  for (int graph = 0; graph < 20; ++graph) {
    const gl::vertex_id n = 40 + rng() % 21; // 40..60
    const auto weighted = random_edges<directed>(rng, n);
    const auto unweighted = drop_weights(weighted);

    const auto csr_w = gl::csr_storage<D, gl::weighted<>>::from_edges(n, weighted);
    const auto mat_w =
      gl::matrix_storage<D, gl::weighted<>>::from_edges(n, weighted);
    const auto csr_u =
      gl::csr_storage<D, gl::unweighted>::from_edges(n, unweighted);
    const auto mat_u =
      gl::matrix_storage<D, gl::unweighted>::from_edges(n, unweighted);

    CAPTURE(graph);
    CAPTURE(n);
    REQUIRE(mat_w.vertex_count() == csr_w.vertex_count());
    REQUIRE(mat_w.has_negative_weight() == csr_w.has_negative_weight());
    for (gl::vertex_id u = 1; u <= n; ++u) {
      CAPTURE(u);
      REQUIRE(neighbors_of(mat_u, u) == neighbors_of(csr_u, u));
      REQUIRE(neighbors_of(mat_w, u) == neighbors_of(csr_w, u));
      REQUIRE(edges_of(mat_w, u) == edges_of(csr_w, u));
    }
  }
}
} // namespace

TEST_CASE("matrix: same neighbors and weights as CSR on random graphs")
{
  std::mt19937 rng(2026);

  SUBCASE("undirected") { check_matrix_matches_csr<gl::undirected>(rng); }
  SUBCASE("directed") { check_matrix_matches_csr<gl::directed>(rng); }
}
