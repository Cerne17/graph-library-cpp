#include "doctest/doctest.h"
#include "graph/algorithms/search.hpp"
#include "graph/policies.hpp"
#include "graph/storage/csr.hpp"
#include "graph/storage/matrix.hpp"
#include "graph/types.hpp"

#include <cstdint>
#include <vector>

namespace {
const std::vector<gl::edge> example_edges{ { 1, 2 },
                                           { 2, 5 },
                                           { 5, 3 },
                                           { 4, 5 },
                                           { 1, 5 } };

using undirected_csr = gl::csr_storage<gl::undirected, gl::unweighted>;
using undirected_matrix = gl::matrix_storage<gl::undirected, gl::unweighted>;
using directed_csr = gl::csr_storage<gl::directed, gl::unweighted>;
using directed_matrix = gl::matrix_storage<gl::directed, gl::unweighted>;
} // namespace

// Expected values: hand-computed on Part 1's example graph, source 1.

TEST_CASE_TEMPLATE("bfs: example graph from vertex 1",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  const auto g = G::from_edges(5, example_edges);
  const auto tree = gl::algorithms::bfs(g, 1);

  CHECK(tree.source == 1);
  CHECK(tree.parent == std::vector<gl::vertex_id>{ 0, 0, 1, 5, 5, 1 });
  CHECK(tree.level == std::vector<std::int32_t>{ -1, 0, 1, 2, 2, 1 });
}

TEST_CASE_TEMPLATE("bfs: a disconnected vertex stays unreached",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  const auto g = G::from_edges(6, example_edges);
  const auto tree = gl::algorithms::bfs(g, 1);

  CHECK(tree.parent[6] == 0);
  CHECK(tree.level[6] == -1);
  CHECK(tree.level[3] == 2); // the rest is unchanged
}

TEST_CASE_TEMPLATE("bfs: a source with no edges reaches only itself",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  const auto g = G::from_edges(6, example_edges);
  const auto tree = gl::algorithms::bfs(g, 6);

  CHECK(tree.level[6] == 0);
  CHECK(tree.parent[6] == 0);
  for (gl::vertex_id v = 1; v <= 5; ++v) {
    CHECK(tree.level[v] == -1);
    CHECK(tree.parent[v] == 0);
  }
}

TEST_CASE_TEMPLATE("bfs: follows edge direction",
                   G,
                   directed_csr,
                   directed_matrix)
{
  // arcs 1->2, 2->5, 5->3, 4->5, 1->5: nothing reaches 4
  const auto g = G::from_edges(5, example_edges);
  const auto tree = gl::algorithms::bfs(g, 1);

  CHECK(tree.parent == std::vector<gl::vertex_id>{ 0, 0, 1, 5, 0, 1 });
  CHECK(tree.level == std::vector<std::int32_t>{ -1, 0, 1, 2, -1, 1 });
}

TEST_CASE_TEMPLATE("dfs: example graph from vertex 1",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  // dives 1 -> 2 -> 5, then 3 and 4
  const auto g = G::from_edges(5, example_edges);
  const auto tree = gl::algorithms::dfs(g, 1);

  CHECK(tree.source == 1);
  CHECK(tree.parent == std::vector<gl::vertex_id>{ 0, 0, 1, 5, 5, 2 });
  CHECK(tree.level == std::vector<std::int32_t>{ -1, 0, 1, 3, 3, 2 });
}

TEST_CASE_TEMPLATE("dfs: from another source",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  // from 3: 3 -> 5 -> 1 -> 2, then back to 5 -> 4
  const auto g = G::from_edges(5, example_edges);
  const auto tree = gl::algorithms::dfs(g, 3);

  CHECK(tree.parent == std::vector<gl::vertex_id>{ 0, 5, 1, 0, 5, 3 });
  CHECK(tree.level == std::vector<std::int32_t>{ -1, 2, 3, 0, 2, 1 });
}

TEST_CASE_TEMPLATE("dfs: a disconnected vertex stays unreached",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  const auto g = G::from_edges(6, example_edges);
  const auto tree = gl::algorithms::dfs(g, 1);

  CHECK(tree.parent[6] == 0);
  CHECK(tree.level[6] == -1);
}

TEST_CASE_TEMPLATE("dfs: follows edge direction",
                   G,
                   directed_csr,
                   directed_matrix)
{
  const auto g = G::from_edges(5, example_edges);
  const auto tree = gl::algorithms::dfs(g, 1);

  CHECK(tree.parent == std::vector<gl::vertex_id>{ 0, 0, 1, 5, 0, 2 });
  CHECK(tree.level == std::vector<std::int32_t>{ -1, 0, 1, 3, -1, 2 });
}

TEST_CASE("bfs and dfs: work on weighted graphs too")
{
  const std::vector<gl::weighted_edge<double>> weighted{
    { 1, 2, 1.2 }, { 2, 5, 2.5 }, { 5, 3, 5.3 }, { 4, 5, 4.5 }, { 1, 5, 1.5 }
  };
  const auto g =
    gl::csr_storage<gl::undirected, gl::weighted<>>::from_edges(5, weighted);

  CHECK(gl::algorithms::bfs(g, 1).level ==
        std::vector<std::int32_t>{ -1, 0, 1, 2, 2, 1 });
  CHECK(gl::algorithms::dfs(g, 1).level ==
        std::vector<std::int32_t>{ -1, 0, 1, 3, 3, 2 });
}
