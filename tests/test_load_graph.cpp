#include "doctest/doctest.h"
#include "graph/graph.hpp"
#include "graph/io/load_graph.hpp"
#include "graph/policies.hpp"
#include "graph/types.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace {
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

// tests/data/weighted_small.txt, by hand
const std::vector<gl::weighted_edge<double>> weighted_small{ { 1, 2, 0.5 },
                                                             { 2, 3, 1.5 } };
const std::vector<gl::edge> unweighted_small{ { 1, 2 }, { 2, 3 } };
} // namespace

TEST_CASE_TEMPLATE("load_graph: weighted file matches from_edges",
                   G,
                   gl::csr_graph<gl::undirected, gl::weighted<>>,
                   gl::matrix_graph<gl::undirected, gl::weighted<>>)
{
  const auto loaded = gl::io::load_graph<G>("tests/data/weighted_small.txt");
  const auto built = G::from_edges(3, weighted_small);

  REQUIRE(loaded.vertex_count() == built.vertex_count());
  for (gl::vertex_id u = 1; u <= built.vertex_count(); ++u) {
    CAPTURE(u);
    CHECK(neighbors_of(loaded, u) == neighbors_of(built, u));
    CHECK(edges_of(loaded, u) == edges_of(built, u));
  }
}

TEST_CASE_TEMPLATE("load_graph: unweighted file matches from_edges",
                   G,
                   gl::csr_graph<gl::directed, gl::unweighted>,
                   gl::matrix_graph<gl::directed, gl::unweighted>)
{
  const auto loaded = gl::io::load_graph<G>("tests/data/unweighted_small.txt");
  const auto built = G::from_edges(3, unweighted_small);

  REQUIRE(loaded.vertex_count() == built.vertex_count());
  for (gl::vertex_id u = 1; u <= built.vertex_count(); ++u) {
    CAPTURE(u);
    CHECK(neighbors_of(loaded, u) == neighbors_of(built, u));
  }
}

TEST_CASE("load_graph: CSR and matrix load the same graph")
{
  const auto csr = gl::io::load_graph<gl::csr_graph<gl::undirected, gl::weighted<>>>(
    "tests/data/weighted_small.txt");
  const auto matrix =
    gl::io::load_graph<gl::matrix_graph<gl::undirected, gl::weighted<>>>(
      "tests/data/weighted_small.txt");

  REQUIRE(csr.vertex_count() == matrix.vertex_count());
  for (gl::vertex_id u = 1; u <= csr.vertex_count(); ++u) {
    CHECK(edges_of(csr, u) == edges_of(matrix, u));
  }
}

TEST_CASE("load_graph: missing file throws")
{
  using G = gl::csr_graph<gl::undirected, gl::unweighted>;
  CHECK_THROWS_AS(gl::io::load_graph<G>("tests/data/does_not_exist.txt"),
                  std::runtime_error);
}

TEST_CASE("load_graph: malformed input passes read_edge_list's error through")
{
  using G = gl::csr_graph<gl::undirected, gl::weighted<>>;
  CHECK_THROWS_WITH_AS(gl::io::load_graph<G>("tests/data/bad_self_loop.txt"),
                       doctest::Contains("self-loop"),
                       std::runtime_error);
}
