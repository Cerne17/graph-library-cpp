// Cross-checks on random graphs (fixed seeds, several sizes and densities).
// The references are independent of every Dijkstra version: the edge list
// itself (shortest-path optimality conditions) and BFS levels.

#include "graph/algorithms/dijkstra.hpp"
#include "graph/algorithms/search.hpp"
#include "graph/graph.hpp"
#include "graph/policies.hpp"
#include "graph/types.hpp"

#include "doctest/doctest.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <random>
#include <vector>

namespace {
using wedge = gl::weighted_edge<double>;

constexpr double inf = std::numeric_limits<double>::infinity();

struct random_graph
{
  gl::vertex_id n;
  std::vector<wedge> edges;
};

// m random edges over 1..n, no self-loops, parallel edges allowed.
// Weights uniform in [0, 10), with about one in ten exactly 0.
random_graph make_random_graph(std::mt19937& rng, gl::vertex_id n, std::size_t m)
{
  std::uniform_int_distribution<gl::vertex_id> pick(1, n);
  std::uniform_real_distribution<double> weight(0.0, 10.0);
  random_graph g{ n, {} };
  while (g.edges.size() < m) {
    const gl::vertex_id u = pick(rng);
    const gl::vertex_id v = pick(rng);
    if (u == v) {
      continue;
    }
    const double w = rng() % 10 == 0 ? 0.0 : weight(rng);
    g.edges.push_back({ u, v, w });
  }
  return g;
}

// sizes from tiny to a few hundred, densities from a forest-like m < n up to
// about half of all pairs
struct shape
{
  gl::vertex_id n;
  std::size_t m;
};
const std::vector<shape> shapes{ { 2, 1 },     { 5, 3 },      { 10, 30 },
                                 { 50, 40 },   { 50, 600 },   { 200, 150 },
                                 { 200, 800 }, { 200, 10000 }, { 400, 1200 } };

// min weight of an edge u->v (parallel edges keep the smallest), +inf if none
std::vector<double> min_weights(const random_graph& rg, bool directed)
{
  const std::size_t side = rg.n + 1;
  std::vector<double> w(side * side, inf);
  for (const auto& e : rg.edges) {
    w[e.u * side + e.v] = std::min(w[e.u * side + e.v], e.weight);
    if (!directed) {
      w[e.v * side + e.u] = std::min(w[e.v * side + e.u], e.weight);
    }
  }
  return w;
}

template<typename SP>
void check_shortest_path_tree(const SP& sp, const random_graph& rg, bool directed)
{
  const gl::vertex_id n = rg.n;
  const std::size_t side = n + 1;
  const auto w = min_weights(rg, directed);

  REQUIRE(sp.dist[sp.source] == 0.0);
  REQUIRE(sp.parent[sp.source] == 0);

  // tree edges are real edges and tight: dist[v] == dist[parent[v]] + w
  for (gl::vertex_id v = 1; v <= n; ++v) {
    CAPTURE(v);
    if (v == sp.source) {
      continue;
    }
    if (sp.dist[v] == inf) {
      CHECK(sp.parent[v] == 0);
      continue;
    }
    const gl::vertex_id p = sp.parent[v];
    REQUIRE(p >= 1);
    REQUIRE(p <= n);
    REQUIRE(w[p * side + v] != inf);
    CHECK(sp.dist[v] == doctest::Approx(sp.dist[p] + w[p * side + v]));
  }

  // no edge can still be relaxed: dist[v] <= dist[u] + w(u, v)
  for (const auto& e : rg.edges) {
    CAPTURE(e.u);
    CAPTURE(e.v);
    CHECK(sp.dist[e.v] <= sp.dist[e.u] + e.weight + 1e-9);
    if (!directed) {
      CHECK(sp.dist[e.u] <= sp.dist[e.v] + e.weight + 1e-9);
    }
  }
}

void check_same_dist(const std::vector<double>& a, const std::vector<double>& b)
{
  REQUIRE(a.size() == b.size());
  for (std::size_t v = 1; v < a.size(); ++v) {
    CAPTURE(v);
    if (a[v] == inf) {
      CHECK(b[v] == inf);
    } else {
      CHECK(a[v] == doctest::Approx(b[v]));
    }
  }
}

template<typename G>
void check_versions_agree(const G& g, gl::vertex_id s)
{
  const auto array = gl::algorithms::dijkstra_array(g, s);
  const auto heap = gl::algorithms::dijkstra_heap(g, s);
  const auto lazy = gl::algorithms::dijkstra_lazy(g, s);
  check_same_dist(array.dist, heap.dist);
  check_same_dist(array.dist, lazy.dist);
}

std::vector<gl::vertex_id> pick_sources(std::mt19937& rng, gl::vertex_id n)
{
  std::uniform_int_distribution<gl::vertex_id> pick(1, n);
  return { 1, n, pick(rng), pick(rng) };
}
} // namespace

TEST_CASE("dijkstra: the three versions agree on dist")
{
  std::mt19937 rng(242);
  for (const auto [n, m] : shapes) {
    const auto rg = make_random_graph(rng, n, m);
    CAPTURE(n);
    CAPTURE(m);
    const auto u_csr =
      gl::csr_graph<gl::undirected, gl::weighted<>>::from_edges(n, rg.edges);
    const auto d_csr =
      gl::csr_graph<gl::directed, gl::weighted<>>::from_edges(n, rg.edges);
    const auto u_matrix =
      gl::matrix_graph<gl::undirected, gl::weighted<>>::from_edges(n, rg.edges);
    const auto d_matrix =
      gl::matrix_graph<gl::directed, gl::weighted<>>::from_edges(n, rg.edges);

    for (const gl::vertex_id s : pick_sources(rng, n)) {
      CAPTURE(s);
      check_versions_agree(u_csr, s);
      check_versions_agree(d_csr, s);
      check_versions_agree(u_matrix, s);
      check_versions_agree(d_matrix, s);
      // CSR keeps parallel edges, the matrix keeps their minimum: same dist
      check_same_dist(gl::algorithms::dijkstra_heap(u_csr, s).dist,
                      gl::algorithms::dijkstra_heap(u_matrix, s).dist);
      check_same_dist(gl::algorithms::dijkstra_heap(d_csr, s).dist,
                      gl::algorithms::dijkstra_heap(d_matrix, s).dist);
    }
  }
}

TEST_CASE("dijkstra: parent is a valid shortest-path tree")
{
  std::mt19937 rng(2420);
  for (const auto [n, m] : shapes) {
    const auto rg = make_random_graph(rng, n, m);
    CAPTURE(n);
    CAPTURE(m);
    const auto u_g =
      gl::csr_graph<gl::undirected, gl::weighted<>>::from_edges(n, rg.edges);
    const auto d_g =
      gl::csr_graph<gl::directed, gl::weighted<>>::from_edges(n, rg.edges);

    for (const gl::vertex_id s : pick_sources(rng, n)) {
      CAPTURE(s);
      check_shortest_path_tree(gl::algorithms::dijkstra_array(u_g, s), rg, false);
      check_shortest_path_tree(gl::algorithms::dijkstra_heap(u_g, s), rg, false);
      check_shortest_path_tree(gl::algorithms::dijkstra_lazy(u_g, s), rg, false);
      check_shortest_path_tree(gl::algorithms::dijkstra_array(d_g, s), rg, true);
      check_shortest_path_tree(gl::algorithms::dijkstra_heap(d_g, s), rg, true);
      check_shortest_path_tree(gl::algorithms::dijkstra_lazy(d_g, s), rg, true);
    }
  }
}

namespace {
template<typename WeightedG, typename UnweightedG>
void check_against_bfs(const WeightedG& wg, const UnweightedG& ug, gl::vertex_id s)
{
  const auto level = gl::algorithms::bfs(ug, s).level;
  const auto runs = { gl::algorithms::dijkstra_array(wg, s),
                      gl::algorithms::dijkstra_heap(wg, s),
                      gl::algorithms::dijkstra_lazy(wg, s) };
  for (const auto& sp : runs) {
    for (gl::vertex_id v = 1; v < level.size(); ++v) {
      CAPTURE(v);
      if (level[v] == -1) {
        CHECK(sp.dist[v] == inf);
      } else {
        CHECK(sp.dist[v] == doctest::Approx(level[v]));
      }
    }
  }
}
} // namespace

TEST_CASE("dijkstra: unit weights reproduce BFS levels")
{
  std::mt19937 rng(24200);
  for (const auto [n, m] : shapes) {
    auto rg = make_random_graph(rng, n, m);
    std::vector<gl::edge> plain;
    for (auto& e : rg.edges) {
      e.weight = 1.0;
      plain.push_back({ e.u, e.v });
    }
    CAPTURE(n);
    CAPTURE(m);

    const auto u_w =
      gl::csr_graph<gl::undirected, gl::weighted<>>::from_edges(n, rg.edges);
    const auto u_plain =
      gl::csr_graph<gl::undirected, gl::unweighted>::from_edges(n, plain);
    const auto d_w =
      gl::matrix_graph<gl::directed, gl::weighted<>>::from_edges(n, rg.edges);
    const auto d_plain =
      gl::csr_graph<gl::directed, gl::unweighted>::from_edges(n, plain);

    for (const gl::vertex_id s : pick_sources(rng, n)) {
      CAPTURE(s);
      check_against_bfs(u_w, u_plain, s);
      check_against_bfs(d_w, d_plain, s);
    }
  }
}
