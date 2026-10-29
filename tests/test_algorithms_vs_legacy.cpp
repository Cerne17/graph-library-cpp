// Differential test: the generic algorithms must reproduce Part 1 exactly, on
// CSR and on the matrix. Part 1 lives in the global namespace (graph.hpp,
// src/*.cpp, linked into the test binary by the Makefile); the new code lives
// in gl::algorithms, so both can be used side by side.

#include "doctest/doctest.h"
#include "graph.hpp"
#include "graph/algorithms/components.hpp"
#include "graph/algorithms/distance.hpp"
#include "graph/algorithms/search.hpp"
#include "graph/algorithms/stats.hpp"
#include "graph/policies.hpp"
#include "graph/storage/csr.hpp"
#include "graph/storage/matrix.hpp"
#include "graph/types.hpp"

#include <cstddef>
#include <random>
#include <vector>

namespace {
using undirected_csr = gl::csr_storage<gl::undirected, gl::unweighted>;
using undirected_matrix = gl::matrix_storage<gl::undirected, gl::unweighted>;

// distinct unordered pairs, no self-loops
std::vector<gl::edge> random_simple_edges(std::mt19937& rng, gl::vertex_id n)
{
  std::vector<bool> used((n + 1) * (n + 1), false);
  std::vector<gl::edge> edges;
  const gl::vertex_id attempts = n / 2 + rng() % (2 * n); // sparse to dense
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

void check_tree(const SearchTree& old_tree,
                const gl::algorithms::search_tree& new_tree,
                gl::vertex_id n)
{
  REQUIRE(new_tree.parent.size() == old_tree.parent.size());
  REQUIRE(new_tree.level.size() == old_tree.level.size());
  for (gl::vertex_id v = 1; v <= n; ++v) {
    CAPTURE(v);
    REQUIRE(new_tree.parent[v] == static_cast<gl::vertex_id>(old_tree.parent[v]));
    REQUIRE(new_tree.level[v] == old_tree.level[v]);
  }
}

template<typename G>
void check_against_legacy(const G& g,
                          const AdjacencyList& old_g,
                          const GraphStats& old_stats,
                          const std::vector<gl::vertex_id>& sources)
{
  const gl::vertex_id n = g.vertex_count();

  for (const gl::vertex_id s : sources) {
    CAPTURE(s);
    check_tree(::bfs(old_g, static_cast<int>(s)), gl::algorithms::bfs(g, s), n);
    check_tree(::dfs(old_g, static_cast<int>(s)), gl::algorithms::dfs(g, s), n);
  }

  // sizes only: Part 1's unstable sort may reorder equal-sized components
  const auto old_components = getComponentsByBreadth(old_g);
  const auto new_components = gl::algorithms::connected_components(g);
  REQUIRE(new_components.size() == old_components.size());
  for (std::size_t i = 0; i < old_components.size(); ++i) {
    CHECK(new_components[i].size() == old_components[i].size());
  }

  CHECK(gl::algorithms::exact_diameter(g) == getExactDiameter(old_g));
  for (const gl::vertex_id u : sources) {
    for (const gl::vertex_id v : sources) {
      CHECK(gl::algorithms::distance(g, u, v) ==
            getDistance(old_g, static_cast<int>(u), static_cast<int>(v)));
    }
  }

  const auto stats = gl::algorithms::compute_degree_stats(g);
  CHECK(stats.vertex_count == static_cast<gl::vertex_id>(old_stats.n));
  CHECK(stats.edge_count == static_cast<gl::edge_index>(old_stats.m));
  CHECK(stats.min_degree == static_cast<gl::edge_index>(old_stats.minDegree));
  CHECK(stats.max_degree == static_cast<gl::edge_index>(old_stats.maxDegree));
  CHECK(stats.average_degree == doctest::Approx(old_stats.avgDegree));
  CHECK(stats.median_degree == doctest::Approx(old_stats.medianDegree));
}
} // namespace

TEST_CASE("algorithms: CSR and matrix reproduce Part 1 on random graphs")
{
  std::mt19937 rng(242);

  for (int graph = 0; graph < 20; ++graph) {
    const gl::vertex_id n = 40 + rng() % 21; // 40..60
    const auto edges = random_simple_edges(rng, n);

    AdjacencyList old_g(static_cast<int>(n));
    for (const auto& e : edges) {
      old_g.addEdge(static_cast<int>(e.u), static_cast<int>(e.v));
    }
    old_g.finalize();
    const GraphStats old_stats = computeStats(old_g);

    const auto csr = undirected_csr::from_edges(n, edges);
    const auto matrix = undirected_matrix::from_edges(n, edges);

    // first, last, middle and two random vertices
    const std::vector<gl::vertex_id> sources{
      1, n, n / 2, static_cast<gl::vertex_id>(1 + rng() % n),
      static_cast<gl::vertex_id>(1 + rng() % n)
    };

    CAPTURE(graph);
    CAPTURE(n);
    CAPTURE(edges.size());
    check_against_legacy(csr, old_g, old_stats, sources);
    check_against_legacy(matrix, old_g, old_stats, sources);
  }
}
