#include "doctest/doctest.h"
#include "graph/concepts.hpp"
#include "graph/policies.hpp"
#include "graph/storage/csr.hpp"
#include "graph/types.hpp"

#include <algorithm>
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
