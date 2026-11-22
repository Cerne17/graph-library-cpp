// Floyd-Warshall on random graphs (fixed seeds) against Bellman-Ford toward
// each target and Dijkstra from each source, every pair, plus path checks:
// every reconstructed path uses existing arcs and its weight is its distance.

#include "graph/algorithms/floyd_warshall.hpp"

#include "graph/algorithms/bellman_ford.hpp"
#include "graph/algorithms/dijkstra.hpp"
#include "graph/algorithms/path.hpp"
#include "graph/graph.hpp"
#include "graph/policies.hpp"
#include "graph/types.hpp"

#include "doctest/doctest.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <random>
#include <vector>

namespace {
using gl::algorithms::floyd_warshall;

using wedge = gl::weighted_edge<double>;
using directed_csr = gl::csr_graph<gl::directed, gl::weighted<>>;

constexpr double inf = std::numeric_limits<double>::infinity();

const std::vector<std::pair<gl::vertex_id, std::size_t>> shapes{
  { 2, 1 }, { 5, 3 }, { 10, 30 }, { 30, 25 }, { 30, 200 }, { 60, 60 }, { 60, 400 }
};

std::vector<wedge> random_arcs(std::mt19937& rng,
                               gl::vertex_id n,
                               std::size_t m,
                               const std::function<double()>& weight)
{
  std::uniform_int_distribution<gl::vertex_id> pick(1, n);
  std::vector<wedge> arcs;
  while (arcs.size() < m) {
    const gl::vertex_id u = pick(rng);
    const gl::vertex_id v = pick(rng);
    if (u != v) {
      arcs.push_back({ u, v, weight() });
    }
  }
  return arcs;
}

std::vector<double> lightest(gl::vertex_id n, const std::vector<wedge>& arcs)
{
  std::vector<double> w((n + 1) * (n + 1), inf);
  for (const auto& e : arcs) {
    double& cell = w[e.u * (n + 1) + e.v];
    cell = std::min(cell, e.weight);
  }
  return w;
}

// every path from row i: starts at i, ends at j, uses existing arcs, weighs
// dist(i, j) (exact: integer weights)
void check_paths(const gl::algorithms::all_pairs<double>& r,
                 gl::vertex_id n,
                 const std::vector<double>& w)
{
  for (gl::vertex_id i = 1; i <= n; ++i) {
    for (gl::vertex_id j = 1; j <= n; ++j) {
      if (r.minus_infinity(i, j) || r.dist(i, j) == inf) {
        continue;
      }
      CAPTURE(i);
      CAPTURE(j);
      const auto path = gl::algorithms::reconstruct_path(r.tree_from(i), j);
      REQUIRE_FALSE(path.empty());
      CHECK(path.front() == i);
      CHECK(path.back() == j);
      double total = 0.0;
      for (std::size_t s = 0; s + 1 < path.size(); ++s) {
        const double arc = w[path[s] * (n + 1) + path[s + 1]];
        REQUIRE(arc != inf);
        total += arc;
      }
      CHECK(total == r.dist(i, j));
    }
  }
}
} // namespace

TEST_CASE("floyd_warshall differential: negative arcs from potentials, against Bellman-Ford")
{
  // w' = w + p(u) - p(v), w >= 0: no negative cycle; integers, exact sums
  std::mt19937 rng(11);
  for (const auto [n, m] : shapes) {
    CAPTURE(n);
    CAPTURE(m);
    std::vector<double> p(n + 1);
    for (auto& x : p) {
      x = static_cast<double>(rng() % 20);
    }
    const auto base =
      random_arcs(rng, n, m, [&] { return static_cast<double>(rng() % 10); });
    std::vector<wedge> arcs;
    for (const auto& e : base) {
      arcs.push_back({ e.u, e.v, e.weight + p[e.u] - p[e.v] });
    }
    const auto g = directed_csr::from_edges(n, arcs);
    const auto r = floyd_warshall(g);

    CHECK_FALSE(r.negative_cycle());
    for (gl::vertex_id j = 1; j <= n; ++j) {
      const auto to_j = gl::algorithms::bellman_ford(g, j);
      for (gl::vertex_id i = 1; i <= n; ++i) {
        CAPTURE(i);
        CAPTURE(j);
        CHECK(r.dist(i, j) == to_j.dist[i]);
      }
    }
    check_paths(r, n, lightest(n, arcs));
  }
}

TEST_CASE("floyd_warshall differential: non-negative graphs, against Dijkstra")
{
  std::mt19937 rng(12);
  std::uniform_real_distribution<double> real(0.0, 10.0);
  for (const auto [n, m] : shapes) {
    CAPTURE(n);
    CAPTURE(m);
    const auto arcs = random_arcs(rng, n, m, [&] { return real(rng); });
    const auto g = directed_csr::from_edges(n, arcs);
    const auto r = floyd_warshall(g);
    for (gl::vertex_id i = 1; i <= n; ++i) {
      const auto from_i = gl::algorithms::dijkstra_heap(g, i).dist;
      for (gl::vertex_id j = 1; j <= n; ++j) {
        CAPTURE(i);
        CAPTURE(j);
        if (from_i[j] == inf) {
          CHECK(r.dist(i, j) == inf);
        } else {
          CHECK(r.dist(i, j) == doctest::Approx(from_i[j]));
        }
      }
    }
  }
}

TEST_CASE("floyd_warshall differential: planted negative cycles, pair by pair")
{
  // integer weights in -3..5: negative cycles are common. minus_infinity must
  // agree with Bellman-Ford's -inf on every pair; the other cells must agree
  // exactly, and the graph-level flags must agree.
  std::mt19937 rng(13);
  for (int trial = 0; trial < 60; ++trial) {
    const gl::vertex_id n = 2 + rng() % 15; // 2..16
    const std::size_t m = rng() % (3 * n);
    const auto arcs = random_arcs(
      rng, n, m, [&] { return static_cast<double>(static_cast<int>(rng() % 9) - 3); });
    CAPTURE(trial);
    const auto g = directed_csr::from_edges(n, arcs);
    const auto r = floyd_warshall(g);

    CHECK(r.negative_cycle() == gl::algorithms::find_negative_cycle(g).found);
    for (gl::vertex_id j = 1; j <= n; ++j) {
      const auto to_j = gl::algorithms::bellman_ford(g, j);
      for (gl::vertex_id i = 1; i <= n; ++i) {
        CAPTURE(i);
        CAPTURE(j);
        CHECK(r.minus_infinity(i, j) == (to_j.dist[i] == -inf));
        if (!r.minus_infinity(i, j)) {
          CHECK(r.dist(i, j) == to_j.dist[i]);
        }
      }
    }
    check_paths(r, n, lightest(n, arcs));
  }
}
