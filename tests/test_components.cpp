#include "doctest/doctest.h"
#include "graph/algorithms/components.hpp"
#include "graph/policies.hpp"
#include "graph/storage/csr.hpp"
#include "graph/storage/matrix.hpp"
#include "graph/types.hpp"

#include <vector>

namespace {
using undirected_csr = gl::csr_storage<gl::undirected, gl::unweighted>;
using undirected_matrix = gl::matrix_storage<gl::undirected, gl::unweighted>;
using directed_csr = gl::csr_storage<gl::directed, gl::unweighted>;
using directed_matrix = gl::matrix_storage<gl::directed, gl::unweighted>;

template<typename G>
concept has_connected_components =
  requires(const G& g) { gl::algorithms::connected_components(g); };

// Part 1's data/multiple_components_graph.txt
const std::vector<gl::edge> multiple_components_edges{
  { 1, 2 }, { 2, 5 }, { 5, 3 }, { 4, 5 },  { 1, 5 },
  { 6, 7 }, { 8, 9 }, { 9, 10 }
};
} // namespace

// components are only defined for undirected graphs
static_assert(has_connected_components<undirected_csr>);
static_assert(has_connected_components<undirected_matrix>);
static_assert(!has_connected_components<directed_csr>);
static_assert(!has_connected_components<directed_matrix>);

TEST_CASE_TEMPLATE("connected_components: Part 1's disconnected graph",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  const auto g = G::from_edges(10, multiple_components_edges);
  const auto components = gl::algorithms::connected_components(g);

  // largest first; inside each, BFS visiting order
  const std::vector<std::vector<gl::vertex_id>> expected{ { 1, 2, 5, 3, 4 },
                                                          { 8, 9, 10 },
                                                          { 6, 7 } };
  CHECK(components == expected);
}

TEST_CASE_TEMPLATE("connected_components: equal sizes keep discovery order",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  const std::vector<gl::edge> edges{ { 3, 4 }, { 1, 2 } };
  const auto g = G::from_edges(6, edges);
  const auto components = gl::algorithms::connected_components(g);

  const std::vector<std::vector<gl::vertex_id>> expected{
    { 1, 2 }, { 3, 4 }, { 5 }, { 6 }
  };
  CHECK(components == expected);
}

TEST_CASE_TEMPLATE("connected_components: connected graph is one component",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  const std::vector<gl::edge> edges{
    { 1, 2 }, { 2, 5 }, { 5, 3 }, { 4, 5 }, { 1, 5 }
  };
  const auto g = G::from_edges(5, edges);
  const auto components = gl::algorithms::connected_components(g);

  REQUIRE(components.size() == 1);
  CHECK(components[0] == std::vector<gl::vertex_id>{ 1, 2, 5, 3, 4 });
}

TEST_CASE_TEMPLATE("connected_components: no edges gives one per vertex",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  const std::vector<gl::edge> no_edges;
  const auto g = G::from_edges(3, no_edges);
  const auto components = gl::algorithms::connected_components(g);

  const std::vector<std::vector<gl::vertex_id>> expected{ { 1 }, { 2 }, { 3 } };
  CHECK(components == expected);
}

TEST_CASE("connected_components: zero vertices")
{
  const std::vector<gl::edge> no_edges;
  const auto g = undirected_csr::from_edges(0, no_edges);

  CHECK(gl::algorithms::connected_components(g).empty());
}
