#include "doctest/doctest.h"
#include "graph/concepts.hpp"
#include "graph/policies.hpp"
#include "graph/storage/csr.hpp"
#include "graph/types.hpp"

#include <algorithm>
#include <concepts>
#include <utility>
#include <vector>

using undirected_unweighted_csr =
  gl::csr_storage<gl::undirected, gl::unweighted>;
using undirected_weighted_csr = gl::csr_storage<gl::undirected, gl::weighted<>>;
using directed_unweighted_csr = gl::csr_storage<gl::directed, gl::unweighted>;
using directed_weighted_csr = gl::csr_storage<gl::directed, gl::weighted<>>;

static_assert(gl::graph_like<undirected_unweighted_csr>);
static_assert(gl::graph_like<directed_unweighted_csr>);
static_assert(!gl::weighted_graph<undirected_unweighted_csr>);
static_assert(gl::weighted_graph<undirected_weighted_csr>);
static_assert(gl::weighted_graph<directed_weighted_csr>);

// policy aliases are forwarded unchanged
static_assert(
  std::same_as<undirected_weighted_csr::direction_type, gl::undirected>);
static_assert(std::same_as<directed_unweighted_csr::direction_type, gl::directed>);
static_assert(
  std::same_as<undirected_weighted_csr::weighting_type, gl::weighted<>>);
static_assert(
  std::same_as<undirected_unweighted_csr::weighting_type, gl::unweighted>);

// G::weight_type exists only on weighted graphs, and matches the policy's
static_assert(std::same_as<undirected_weighted_csr::weight_type, double>);
static_assert(
  std::same_as<gl::csr_storage<gl::undirected, gl::weighted<float>>::weight_type,
               float>);
static_assert(!gl::has_weight_type<undirected_unweighted_csr>);
static_assert(!gl::has_weight_type<directed_unweighted_csr>);

// input_edge picks the edge struct that matches the weighting
static_assert(std::same_as<undirected_unweighted_csr::input_edge, gl::edge>);
static_assert(std::same_as<directed_weighted_csr::input_edge,
                           gl::weighted_edge<double>>);

namespace {
template<typename G>
concept has_weights = requires(const G& g) { g.weights(); };

template<typename G>
concept has_negative_weight_query =
  requires(const G& g) { g.has_negative_weight(); };
} // namespace

// weight-only members are rejected on unweighted graphs
static_assert(has_weights<undirected_weighted_csr>);
static_assert(!has_weights<undirected_unweighted_csr>);
static_assert(has_negative_weight_query<directed_weighted_csr>);
static_assert(!has_negative_weight_query<directed_unweighted_csr>);

namespace {
const std::vector<gl::edge> example_edges{ { 1, 2 },
                                           { 2, 5 },
                                           { 5, 3 },
                                           { 4, 5 },
                                           { 1, 5 } };
const std::vector<gl::weighted_edge<double>> example_weighted_edges{
  { 1, 2, 1.2 },
  { 2, 5, 2.5 },
  { 5, 3, 5.3 },
  { 4, 5, 4.5 },
  { 1, 5, 1.5 }
};

const std::vector<gl::weighted_edge<double>> example_directed_weighted_edges{
  { 1, 2, 1.2 }, { 2, 5, 2.5 }, { 5, 3, 5.3 },
  { 4, 5, 4.5 }, { 1, 5, 1.5 }, { 5, 1, 5.1 }
};
} // namespace

TEST_CASE(
  "csr: undirected unweighted example graph matches the hand-computed layout")
{
  const auto s = undirected_unweighted_csr::from_edges(5, example_edges);

  const std::vector<gl::edge_index> expected_offsets_s{ 0, 0, 2, 4, 5, 6, 10 };
  const std::vector<gl::vertex_id> expected_targets_s{ 2, 5, 1, 5, 5,
                                                       5, 1, 2, 3, 4 };

  std::vector<gl::vertex_id> neighbors_5_s;
  std::vector<gl::vertex_id> expected_neighbors_5_s{ 1, 2, 3, 4 };
  s.for_each_neighbor(5, [&](gl::vertex_id v) { neighbors_5_s.push_back(v); });

  CHECK(s.vertex_count() == 5);
  CHECK(std::ranges::equal(s.offsets(), expected_offsets_s));
  CHECK(std::ranges::equal(s.targets(), expected_targets_s));
  CHECK(std::ranges::equal(neighbors_5_s, expected_neighbors_5_s));
}

TEST_CASE(
  "csr: undirected weighted example graph matches the hand-computed layout")
{
  const auto s = undirected_weighted_csr::from_edges(5, example_weighted_edges);

  const std::vector<gl::edge_index> expected_offsets_s{ 0, 0, 2, 4, 5, 6, 10 };
  const std::vector<gl::vertex_id> expected_targets_s{ 2, 5, 1, 5, 5,
                                                       5, 1, 2, 3, 4 };
  const std::vector<double> expected_weights_s{ 1.2, 1.5, 1.2, 2.5, 5.3,
                                                4.5, 1.5, 2.5, 5.3, 4.5 };

  std::vector<gl::vertex_id> neighbors_5_s;
  std::vector<gl::vertex_id> expected_neighbors_5_s{ 1, 2, 3, 4 };
  s.for_each_neighbor(5, [&](gl::vertex_id v) { neighbors_5_s.push_back(v); });

  CHECK(s.vertex_count() == 5);
  CHECK(std::ranges::equal(s.offsets(), expected_offsets_s));
  CHECK(std::ranges::equal(s.targets(), expected_targets_s));
  CHECK(std::ranges::equal(s.weights(), expected_weights_s));
  CHECK(std::ranges::equal(neighbors_5_s, expected_neighbors_5_s));
}

TEST_CASE(
  "csr: directed unweighted example graph matched the hand-computed layout")
{
  const auto t = directed_unweighted_csr::from_edges(5, example_edges);

  const std::vector<gl::edge_index> expected_offsets_t{ 0, 0, 2, 3, 3, 4, 5 };
  const std::vector<gl::vertex_id> expected_targets_t{ 2, 5, 5, 5, 3 };

  std::vector<gl::vertex_id> neighbors_5_t;
  std::vector<gl::vertex_id> expected_neighbors_5_t{ 3 };
  t.for_each_neighbor(5, [&](gl::vertex_id v) { neighbors_5_t.push_back(v); });

  CHECK(t.vertex_count() == 5);
  CHECK(std::ranges::equal(t.offsets(), expected_offsets_t));
  CHECK(std::ranges::equal(t.targets(), expected_targets_t));
  CHECK(std::ranges::equal(neighbors_5_t, expected_neighbors_5_t));
}

TEST_CASE(
  "csr: directed weighted example graph matched the hand-computed layout")
{
  const auto t =
    directed_weighted_csr::from_edges(5, example_directed_weighted_edges);

  const std::vector<gl::edge_index> expected_offsets_t{ 0, 0, 2, 3, 3, 4, 6 };
  const std::vector<gl::vertex_id> expected_targets_t{ 2, 5, 5, 5, 1, 3 };
  const std::vector<double> expected_weights_t{ 1.2, 1.5, 2.5, 4.5, 5.1, 5.3 };

  std::vector<gl::vertex_id> neighbors_5_t;
  std::vector<gl::vertex_id> expected_neighbors_5_t{ 1, 3 };
  t.for_each_neighbor(5, [&](gl::vertex_id v) { neighbors_5_t.push_back(v); });

  CHECK(t.vertex_count() == 5);
  CHECK(t.offsets().size() == 7);
  CHECK(t.targets().size() == 6);
  CHECK(t.weights().size() == 6);
  CHECK(std::ranges::equal(t.offsets(), expected_offsets_t));
  CHECK(std::ranges::equal(t.targets(), expected_targets_t));
  CHECK(std::ranges::equal(t.weights(), expected_weights_t));
  CHECK(std::ranges::equal(neighbors_5_t, expected_neighbors_5_t));
}

TEST_CASE("csr: isolated vertex in the undirected unweighted graph")
{
  const auto s = undirected_unweighted_csr::from_edges(6, example_edges);

  const std::vector<gl::edge_index> expected_offsets_s{
    0, 0, 2, 4, 5, 6, 10, 10
  };
  const std::vector<gl::vertex_id> expected_targets_s{ 2, 5, 1, 5, 5,
                                                       5, 1, 2, 3, 4 };

  std::vector<gl::vertex_id> neighbors_5_s;
  std::vector<gl::vertex_id> expected_neighbors_5_s{ 1, 2, 3, 4 };
  s.for_each_neighbor(5, [&](gl::vertex_id v) { neighbors_5_s.push_back(v); });

  std::vector<gl::vertex_id> neighbors_6_s;
  std::vector<gl::vertex_id> expected_neighbors_6_s{};
  s.for_each_neighbor(6, [&](gl::vertex_id v) { neighbors_6_s.push_back(v); });

  CHECK(s.vertex_count() == 6);
  CHECK(std::ranges::equal(s.offsets(), expected_offsets_s));
  CHECK(std::ranges::equal(s.targets(), expected_targets_s));
  CHECK(std::ranges::equal(neighbors_5_s, expected_neighbors_5_s));
  CHECK(std::ranges::equal(neighbors_6_s, expected_neighbors_6_s));
}

TEST_CASE("csr: for_each_edge visits neighbors with their weights (undirected)")
{
  const auto s = undirected_weighted_csr::from_edges(5, example_weighted_edges);

  std::vector<std::pair<gl::vertex_id, double>> edges_5_s;
  const std::vector<std::pair<gl::vertex_id, double>> expected_edges_5_s{
    { 1, 1.5 }, { 2, 2.5 }, { 3, 5.3 }, { 4, 4.5 }
  };
  s.for_each_edge(
    5, [&](gl::vertex_id v, double w) { edges_5_s.emplace_back(v, w); });

  CHECK(std::ranges::equal(edges_5_s, expected_edges_5_s));
}

TEST_CASE("csr: for_each_edge follows edge direction (directed)")
{
  const auto t =
    directed_weighted_csr::from_edges(5, example_directed_weighted_edges);

  std::vector<std::pair<gl::vertex_id, double>> edges_1_t;
  const std::vector<std::pair<gl::vertex_id, double>> expected_edges_1_t{
    { 2, 1.2 }, { 5, 1.5 }
  };
  t.for_each_edge(
    1, [&](gl::vertex_id v, double w) { edges_1_t.emplace_back(v, w); });

  std::vector<std::pair<gl::vertex_id, double>> edges_3_t;
  t.for_each_edge(
    3, [&](gl::vertex_id v, double w) { edges_3_t.emplace_back(v, w); });

  CHECK(std::ranges::equal(edges_1_t, expected_edges_1_t));
  CHECK(edges_3_t.empty()); // 3 only receives edges
}

TEST_CASE("csr: has_negative_weight reports negative input weights")
{
  const std::vector<gl::weighted_edge<double>> negative_edges{
    { 1, 2, -1.0 }, { 2, 3, 2.0 }
  };

  const auto s = undirected_weighted_csr::from_edges(5, example_weighted_edges);
  const auto n = undirected_weighted_csr::from_edges(3, negative_edges);

  CHECK_FALSE(s.has_negative_weight());
  CHECK(n.has_negative_weight());
}

TEST_CASE("csr: directed graph keeps out-edges only")
{
  const auto t = directed_unweighted_csr::from_edges(5, example_edges);

  std::vector<gl::vertex_id> neighbors_2_t;
  const std::vector<gl::vertex_id> expected_neighbors_2_t{ 5 }; // not 1
  t.for_each_neighbor(2, [&](gl::vertex_id v) { neighbors_2_t.push_back(v); });

  std::vector<gl::vertex_id> neighbors_3_t;
  t.for_each_neighbor(3, [&](gl::vertex_id v) { neighbors_3_t.push_back(v); });

  CHECK(std::ranges::equal(neighbors_2_t, expected_neighbors_2_t));
  CHECK(neighbors_3_t.empty()); // sink: in-degree 1, out-degree 0
}

TEST_CASE("csr: graph without edges has empty blocks for every vertex")
{
  const std::vector<gl::edge> no_edges;
  const auto s = undirected_unweighted_csr::from_edges(4, no_edges);
  const auto t = directed_unweighted_csr::from_edges(4, no_edges);

  const std::vector<gl::edge_index> expected_offsets{ 0, 0, 0, 0, 0, 0 };

  CHECK(s.vertex_count() == 4);
  CHECK(std::ranges::equal(s.offsets(), expected_offsets));
  CHECK(s.targets().empty());
  CHECK(std::ranges::equal(t.offsets(), expected_offsets));
  CHECK(t.targets().empty());

  for (gl::vertex_id u = 1; u <= 4; ++u) {
    bool visited = false;
    s.for_each_neighbor(u, [&](gl::vertex_id) { visited = true; });
    CHECK_FALSE(visited);
  }
}

TEST_CASE("csr: undirected layout does not depend on edge order or orientation")
{
  // example_weighted_edges reversed, each edge written as (v, u)
  const std::vector<gl::weighted_edge<double>> shuffled_edges{
    { 5, 1, 1.5 }, { 5, 4, 4.5 }, { 3, 5, 5.3 }, { 5, 2, 2.5 }, { 2, 1, 1.2 }
  };

  const auto a = undirected_weighted_csr::from_edges(5, example_weighted_edges);
  const auto b = undirected_weighted_csr::from_edges(5, shuffled_edges);

  CHECK(std::ranges::equal(a.offsets(), b.offsets()));
  CHECK(std::ranges::equal(a.targets(), b.targets()));
  CHECK(std::ranges::equal(a.weights(), b.weights())); // weights follow sort
}

TEST_CASE("csr: parallel edges are kept")
{
  const std::vector<gl::edge> parallel_edges{ { 1, 2 }, { 1, 2 } };
  const auto s = undirected_unweighted_csr::from_edges(2, parallel_edges);

  const std::vector<gl::edge_index> expected_offsets_s{ 0, 0, 2, 4 };
  const std::vector<gl::vertex_id> expected_targets_s{ 2, 2, 1, 1 };

  CHECK(std::ranges::equal(s.offsets(), expected_offsets_s));
  CHECK(std::ranges::equal(s.targets(), expected_targets_s));
}
