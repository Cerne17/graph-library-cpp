#include "doctest/doctest.h"
#include "graph/concepts.hpp"
#include "graph/policies.hpp"
#include "graph/storage/csr.hpp"
#include "graph/types.hpp"

#include <algorithm>
#include <vector>

using undirected_csr = gl::csr_storage<gl::undirected, gl::unweighted>;
using directed_csr = gl::csr_storage<gl::directed, gl::unweighted>;

static_assert(gl::graph_like<undirected_csr>);
static_assert(gl::graph_like<directed_csr>);
static_assert(!gl::weighted_graph<undirected_csr>);

namespace {
const std::vector<gl::edge> example_edges{ { 1, 2 },
                                           { 2, 5 },
                                           { 5, 3 },
                                           { 4, 5 },
                                           { 1, 5 } };
} // namespace

TEST_CASE("csr: undirected example graph matches the hand-computed layout")
{
  const auto s = undirected_csr::from_edges(5, example_edges);

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

TEST_CASE("csr: directed example graph matched the hand-computed layout")
{
  const auto t = directed_csr::from_edges(5, example_edges);

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

TEST_CASE("csr: isolated vertex in the undirected graph")
{
  const auto s = undirected_csr::from_edges(6, example_edges);

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
  s.for_each_neighbor(6, [&](gl::vertex_id v) { neighbors_5_s.push_back(v); });

  CHECK(s.vertex_count() == 6);
  CHECK(std::ranges::equal(s.offsets(), expected_offsets_s));
  CHECK(std::ranges::equal(s.targets(), expected_targets_s));
  CHECK(std::ranges::equal(neighbors_5_s, expected_neighbors_5_s));
  CHECK(std::ranges::equal(neighbors_6_s, expected_neighbors_6_s));
}
