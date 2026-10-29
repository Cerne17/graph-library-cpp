#include "doctest/doctest.h"
#include "graph/algorithms/stats.hpp"
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

const std::vector<gl::edge> example_edges{ { 1, 2 },
                                           { 2, 5 },
                                           { 5, 3 },
                                           { 4, 5 },
                                           { 1, 5 } };
} // namespace

TEST_CASE_TEMPLATE("degree stats: Part 1's example graph",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  // degrees 2, 2, 1, 1, 4 -> sorted 1, 1, 2, 2, 4
  const auto g = G::from_edges(5, example_edges);
  const auto stats = gl::algorithms::compute_degree_stats(g);

  CHECK(stats.vertex_count == 5);
  CHECK(stats.edge_count == 5);
  CHECK(stats.min_degree == 1);
  CHECK(stats.max_degree == 4);
  CHECK(stats.average_degree == doctest::Approx(2.0));
  CHECK(stats.median_degree == doctest::Approx(2.0));
}

TEST_CASE_TEMPLATE("degree stats: even n averages the two middle degrees",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  // vertex 6 isolated: degrees 0, 1, 1, 2, 2, 4
  const auto g = G::from_edges(6, example_edges);
  const auto stats = gl::algorithms::compute_degree_stats(g);

  CHECK(stats.vertex_count == 6);
  CHECK(stats.edge_count == 5);
  CHECK(stats.min_degree == 0);
  CHECK(stats.max_degree == 4);
  CHECK(stats.average_degree == doctest::Approx(10.0 / 6.0));
  CHECK(stats.median_degree == doctest::Approx(1.5));
}

TEST_CASE_TEMPLATE("degree stats: directed graphs count out-degrees and arcs",
                   G,
                   directed_csr,
                   directed_matrix)
{
  // out-degrees 2, 1, 0, 1, 1 -> sorted 0, 1, 1, 1, 2
  const auto g = G::from_edges(5, example_edges);
  const auto stats = gl::algorithms::compute_degree_stats(g);

  CHECK(stats.edge_count == 5);
  CHECK(stats.min_degree == 0);
  CHECK(stats.max_degree == 2);
  CHECK(stats.average_degree == doctest::Approx(1.0));
  CHECK(stats.median_degree == doctest::Approx(1.0));
}

TEST_CASE("degree stats: graph without edges")
{
  const std::vector<gl::edge> no_edges;
  const auto g = undirected_csr::from_edges(4, no_edges);
  const auto stats = gl::algorithms::compute_degree_stats(g);

  CHECK(stats.edge_count == 0);
  CHECK(stats.min_degree == 0);
  CHECK(stats.max_degree == 0);
  CHECK(stats.average_degree == doctest::Approx(0.0));
  CHECK(stats.median_degree == doctest::Approx(0.0));
}
