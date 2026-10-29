#include "doctest/doctest.h"
#include "graph/algorithms/distance.hpp"
#include "graph/policies.hpp"
#include "graph/storage/csr.hpp"
#include "graph/storage/matrix.hpp"
#include "graph/types.hpp"

#include <random>
#include <vector>

namespace {
using undirected_csr = gl::csr_storage<gl::undirected, gl::unweighted>;
using undirected_matrix = gl::matrix_storage<gl::undirected, gl::unweighted>;
using directed_csr = gl::csr_storage<gl::directed, gl::unweighted>;
using directed_matrix = gl::matrix_storage<gl::directed, gl::unweighted>;

template<typename G>
concept has_approximate_diameter =
  requires(const G& g) { gl::algorithms::approximate_diameter(g); };

const std::vector<gl::edge> example_edges{ { 1, 2 },
                                           { 2, 5 },
                                           { 5, 3 },
                                           { 4, 5 },
                                           { 1, 5 } };
const std::vector<gl::edge> multiple_components_edges{
  { 1, 2 }, { 2, 5 }, { 5, 3 }, { 4, 5 },  { 1, 5 },
  { 6, 7 }, { 8, 9 }, { 9, 10 }
};
} // namespace

// double sweep relies on components, so it is undirected only
static_assert(has_approximate_diameter<undirected_csr>);
static_assert(!has_approximate_diameter<directed_csr>);
static_assert(!has_approximate_diameter<directed_matrix>);

TEST_CASE_TEMPLATE("distance: example graph",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  const auto g = G::from_edges(5, example_edges);

  CHECK(gl::algorithms::distance(g, 1, 1) == 0);
  CHECK(gl::algorithms::distance(g, 1, 5) == 1);
  CHECK(gl::algorithms::distance(g, 1, 3) == 2);
  CHECK(gl::algorithms::distance(g, 3, 4) == 2);
  CHECK(gl::algorithms::distance(g, 4, 3) == 2);
}

TEST_CASE_TEMPLATE("distance: unreachable is -1",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  const auto g = G::from_edges(10, multiple_components_edges);

  CHECK(gl::algorithms::distance(g, 1, 6) == -1);
  CHECK(gl::algorithms::distance(g, 8, 10) == 2);
}

TEST_CASE_TEMPLATE("distance: follows edge direction",
                   G,
                   directed_csr,
                   directed_matrix)
{
  const auto g = G::from_edges(5, example_edges);

  CHECK(gl::algorithms::distance(g, 1, 3) == 2);
  CHECK(gl::algorithms::distance(g, 3, 1) == -1);
  CHECK(gl::algorithms::distance(g, 4, 1) == -1);
}

TEST_CASE_TEMPLATE("diameters: Part 1's small graphs",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  const auto example = G::from_edges(5, example_edges);
  const auto multiple = G::from_edges(10, multiple_components_edges);

  CHECK(gl::algorithms::exact_diameter(example) == 2);
  CHECK(gl::algorithms::approximate_diameter(example) == 2);
  CHECK(gl::algorithms::exact_diameter(multiple) == 2);
  CHECK(gl::algorithms::approximate_diameter(multiple) == 2);
}

TEST_CASE_TEMPLATE("diameters: path graphs",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  // 1-2-3-4-5, and 2-3-1-4-5 with vertex 1 in the middle of the path
  const std::vector<gl::edge> in_order{ { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 } };
  const std::vector<gl::edge> scrambled{
    { 2, 3 }, { 3, 1 }, { 1, 4 }, { 4, 5 }
  };
  const auto a = G::from_edges(5, in_order);
  const auto b = G::from_edges(5, scrambled);

  CHECK(gl::algorithms::exact_diameter(a) == 4);
  CHECK(gl::algorithms::approximate_diameter(a) == 4);
  CHECK(gl::algorithms::exact_diameter(b) == 4);
  CHECK(gl::algorithms::approximate_diameter(b) == 4);
}

TEST_CASE_TEMPLATE("diameters: a smaller component can hold the diameter",
                   G,
                   undirected_csr,
                   undirected_matrix)
{
  // the 4-clique 1-2-3-4 (diameter 1) is the largest component, but the
  // smaller path 5-6-7 (diameter 2) holds the diameter
  const std::vector<gl::edge> edges{ { 1, 2 }, { 1, 3 }, { 1, 4 },
                                     { 2, 3 }, { 2, 4 }, { 3, 4 },
                                     { 5, 6 }, { 6, 7 } };
  const auto g = G::from_edges(7, edges);

  CHECK(gl::algorithms::exact_diameter(g) == 2);       // the path 5-6-7
  CHECK(gl::algorithms::approximate_diameter(g) == 1); // sweeps the clique
}

TEST_CASE("diameters: single vertex and no edges")
{
  const std::vector<gl::edge> no_edges;
  const auto one = undirected_csr::from_edges(1, no_edges);
  const auto three = undirected_csr::from_edges(3, no_edges);

  CHECK(gl::algorithms::exact_diameter(one) == 0);
  CHECK(gl::algorithms::approximate_diameter(one) == 0);
  CHECK(gl::algorithms::exact_diameter(three) == 0);
  CHECK(gl::algorithms::approximate_diameter(three) == 0);
}

TEST_CASE_TEMPLATE("exact_diameter: directed graph",
                   G,
                   directed_csr,
                   directed_matrix)
{
  // longest finite directed distances: 1->3, 2->3 and 4->3, all 2
  const auto g = G::from_edges(5, example_edges);

  CHECK(gl::algorithms::exact_diameter(g) == 2);
}

namespace {
// distinct unordered pairs, no self-loops
std::vector<gl::edge> random_simple_edges(std::mt19937& rng,
                                          gl::vertex_id n,
                                          gl::vertex_id attempts)
{
  std::vector<bool> used((n + 1) * (n + 1), false);
  std::vector<gl::edge> edges;
  for (gl::vertex_id i = 0; i < attempts; ++i) {
    const gl::vertex_id u = 1 + rng() % n;
    const gl::vertex_id v = 1 + rng() % n;
    if (u == v || used[u * (n + 1) + v]) {
      continue;
    }
    used[u * (n + 1) + v] = true;
    used[v * (n + 1) + u] = true;
    edges.push_back({ u, v });
  }
  return edges;
}
} // namespace

TEST_CASE("approximate_diameter: a lower bound on random graphs")
{
  std::mt19937 rng(11);
  for (int graph = 0; graph < 30; ++graph) {
    const gl::vertex_id n = 20 + rng() % 41; // 20..60
    const auto edges = random_simple_edges(rng, n, n + rng() % (2 * n));
    const auto g = undirected_csr::from_edges(n, edges);

    CAPTURE(graph);
    CHECK(gl::algorithms::approximate_diameter(g) <=
          gl::algorithms::exact_diameter(g));
  }
}

TEST_CASE("approximate_diameter: exact on random trees")
{
  std::mt19937 rng(12);
  for (int graph = 0; graph < 30; ++graph) {
    const gl::vertex_id n = 2 + rng() % 59; // 2..60
    std::vector<gl::edge> edges;
    for (gl::vertex_id v = 2; v <= n; ++v) {
      edges.push_back({ v, static_cast<gl::vertex_id>(1 + rng() % (v - 1)) });
    }
    const auto g = undirected_csr::from_edges(n, edges);

    CAPTURE(graph);
    CHECK(gl::algorithms::approximate_diameter(g) ==
          gl::algorithms::exact_diameter(g));
  }
}
