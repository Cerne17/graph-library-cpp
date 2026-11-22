// Bellman-Ford on random graphs (fixed seeds) against references that do not
// use it: Dijkstra on non-negative graphs, graphs reweighted by potentials
// (true distances known from Dijkstra), planted negative cycles checked with
// BFS reachability, and brute force over simple paths and cycles for n <= 6.

#include "graph/algorithms/bellman_ford.hpp"

#include "graph/algorithms/dijkstra.hpp"
#include "graph/algorithms/search.hpp"
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
using gl::algorithms::bellman_ford;
using gl::algorithms::bellman_ford_stats;
using gl::algorithms::bellman_ford_variant;
using gl::algorithms::find_negative_cycle;

using wedge = gl::weighted_edge<double>;
using directed_csr = gl::csr_graph<gl::directed, gl::weighted<>>;
using plain_csr = gl::csr_graph<gl::directed, gl::unweighted>;

constexpr double inf = std::numeric_limits<double>::infinity();

struct shape
{
  gl::vertex_id n;
  std::size_t m;
};
const std::vector<shape> shapes{ { 2, 1 },    { 5, 3 },     { 10, 30 },
                                 { 50, 40 },  { 50, 600 },  { 200, 150 },
                                 { 200, 800 } };

// m random arcs over 1..n, no self-loops, parallel arcs allowed.
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

std::vector<wedge> swapped(const std::vector<wedge>& arcs)
{
  std::vector<wedge> out;
  for (const auto& e : arcs) {
    out.push_back({ e.v, e.u, e.weight });
  }
  return out;
}

std::vector<gl::edge> unweighted(const std::vector<wedge>& arcs)
{
  std::vector<gl::edge> out;
  for (const auto& e : arcs) {
    out.push_back({ e.u, e.v });
  }
  return out;
}

// lightest arc u -> v, +inf if none
std::vector<double> lightest(gl::vertex_id n, const std::vector<wedge>& arcs)
{
  std::vector<double> w((n + 1) * (n + 1), inf);
  for (const auto& e : arcs) {
    double& cell = w[e.u * (n + 1) + e.v];
    cell = std::min(cell, e.weight);
  }
  return w;
}

// next[] forms a shortest-path tree toward the target on every finite vertex.
void check_tree(const gl::algorithms::target_paths<double>& t,
                gl::vertex_id n,
                const std::vector<double>& w)
{
  for (gl::vertex_id v = 1; v <= n; ++v) {
    if (v == t.target || t.dist[v] == inf || t.dist[v] == -inf) {
      continue;
    }
    CAPTURE(v);
    const gl::vertex_id after = t.next[v];
    REQUIRE(after != 0);
    CHECK(t.dist[v] ==
          doctest::Approx(w[v * (n + 1) + after] + t.dist[after]));
  }
}
} // namespace

TEST_CASE("bellman_ford differential: matches Dijkstra on non-negative graphs")
{
  std::mt19937 rng(20261122);
  std::uniform_real_distribution<double> real(0.0, 10.0);
  const auto weight = [&] { return rng() % 10 == 0 ? 0.0 : real(rng); };

  for (const auto [n, m] : shapes) {
    CAPTURE(n);
    CAPTURE(m);
    const auto arcs = random_arcs(rng, n, m, weight);
    const auto g = directed_csr::from_edges(n, arcs);
    const auto w = lightest(n, arcs);

    std::vector<std::vector<double>> from(n + 1);
    for (gl::vertex_id s = 1; s <= n; ++s) {
      from[s] = gl::algorithms::dijkstra_heap(g, s).dist;
    }
    for (gl::vertex_id t = 1; t <= n; ++t) {
      const auto to = bellman_ford(g, t);
      CHECK_FALSE(to.cycle_reaches_target);
      for (gl::vertex_id s = 1; s <= n; ++s) {
        if (from[s][t] == inf) {
          CHECK(to.dist[s] == inf);
        } else {
          CHECK(to.dist[s] == doctest::Approx(from[s][t]));
        }
      }
      check_tree(to, n, w);
    }
  }
}

TEST_CASE("bellman_ford differential: negative arcs from potentials")
{
  // w' = w + p(u) - p(v) with w >= 0 keeps every cycle at its original
  // (non-negative) weight, so there is no negative cycle, and
  // dist'(u -> t) = dist_w(u -> t) + p(u) - p(t). Integers: exact sums.
  std::mt19937 rng(7);
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
    // dist_w(u -> t) = Dijkstra from t on the arcs turned around
    const auto back = directed_csr::from_edges(n, swapped(base));

    CHECK_FALSE(find_negative_cycle(g).found);
    for (gl::vertex_id t = 1; t <= n; ++t) {
      const auto to = bellman_ford(g, t);
      const auto ref = gl::algorithms::dijkstra_heap(back, t).dist;
      CHECK_FALSE(to.cycle_reaches_target);
      for (gl::vertex_id u = 1; u <= n; ++u) {
        CAPTURE(u);
        CHECK(to.dist[u] == (ref[u] == inf ? inf : ref[u] + p[u] - p[t]));
      }
    }
  }
}

TEST_CASE("bellman_ford differential: planted negative cycle")
{
  // a random non-negative graph plus a -> b (2), b -> c (2), c -> a (-5).
  // Every negative cycle uses a planted arc, so a vertex reaches a negative
  // cycle exactly when it reaches a, and that cycle reaches t exactly when a
  // reaches t.
  std::mt19937 rng(99);
  for (const auto [n, m] : shapes) {
    if (n < 3) {
      continue;
    }
    CAPTURE(n);
    CAPTURE(m);
    auto arcs = random_arcs(rng, n, m, [&] { return static_cast<double>(rng() % 10); });
    std::uniform_int_distribution<gl::vertex_id> pick(1, n);
    gl::vertex_id a = pick(rng);
    gl::vertex_id b = pick(rng);
    gl::vertex_id c = pick(rng);
    while (b == a) {
      b = pick(rng);
    }
    while (c == a || c == b) {
      c = pick(rng);
    }
    arcs.push_back({ a, b, 2.0 });
    arcs.push_back({ b, c, 2.0 });
    arcs.push_back({ c, a, -5.0 });

    const auto g = directed_csr::from_edges(n, arcs);
    const auto w = lightest(n, arcs);
    const auto fwd = plain_csr::from_edges(n, unweighted(arcs));
    const auto back = plain_csr::from_edges(n, unweighted(swapped(arcs)));
    const auto from_a = gl::algorithms::bfs(fwd, a);
    const auto to_a = gl::algorithms::bfs(back, a);

    const auto report = find_negative_cycle(g);
    REQUIRE(report.found);
    double total = 0.0;
    for (std::size_t i = 0; i < report.cycle.size(); ++i) {
      const auto from = report.cycle[i];
      const auto to = report.cycle[(i + 1) % report.cycle.size()];
      REQUIRE(w[from * (n + 1) + to] != inf); // the arc exists
      total += w[from * (n + 1) + to];
    }
    CHECK(total < 0.0);

    for (gl::vertex_id t = 1; t <= std::min<gl::vertex_id>(n, 10); ++t) {
      CAPTURE(t);
      const auto to = bellman_ford(g, t);
      const bool reaches = from_a.level[t] >= 0;
      CHECK(to.cycle_reaches_target == reaches);
      if (reaches) {
        for (gl::vertex_id v = 1; v <= n; ++v) {
          CAPTURE(v);
          CHECK((to.dist[v] == -inf) == (to_a.level[v] >= 0));
        }
      }
      check_tree(to, n, w);
    }
  }
}

namespace {
// Brute force on tiny graphs, with integer weights.
struct brute
{
  gl::vertex_id n;
  std::vector<double> w; // lightest arc, (n + 1)^2

  double arc(gl::vertex_id u, gl::vertex_id v) const { return w[u * (n + 1) + v]; }

  bool reaches(gl::vertex_id from, gl::vertex_id to) const
  {
    std::vector<bool> seen(n + 1, false);
    std::vector<gl::vertex_id> stack{ from };
    seen[from] = true;
    while (!stack.empty()) {
      const auto u = stack.back();
      stack.pop_back();
      if (u == to) {
        return true;
      }
      for (gl::vertex_id v = 1; v <= n; ++v) {
        if (!seen[v] && arc(u, v) != inf) {
          seen[v] = true;
          stack.push_back(v);
        }
      }
    }
    return false;
  }

  // on_negative_cycle[v]: v lies on some simple cycle of negative weight
  std::vector<bool> on_negative_cycle() const
  {
    std::vector<bool> on(n + 1, false);
    std::vector<gl::vertex_id> path;
    std::vector<bool> used(n + 1, false);
    std::function<void(gl::vertex_id, gl::vertex_id, double)> walk =
      [&](gl::vertex_id start, gl::vertex_id u, double sum) {
        for (gl::vertex_id v = 1; v <= n; ++v) {
          if (arc(u, v) == inf) {
            continue;
          }
          if (v == start && sum + arc(u, v) < 0) {
            for (const auto x : path) {
              on[x] = true;
            }
          } else if (!used[v]) {
            used[v] = true;
            path.push_back(v);
            walk(start, v, sum + arc(u, v));
            path.pop_back();
            used[v] = false;
          }
        }
      };
    for (gl::vertex_id s = 1; s <= n; ++s) {
      used.assign(n + 1, false);
      used[s] = true;
      path = { s };
      walk(s, s, 0.0);
    }
    return on;
  }

  // lightest simple path u -> t, +inf if none
  double simple_path(gl::vertex_id u, gl::vertex_id t) const
  {
    double best = inf;
    std::vector<bool> used(n + 1, false);
    std::function<void(gl::vertex_id, double)> walk = [&](gl::vertex_id x, double sum) {
      if (x == t) {
        best = std::min(best, sum);
        return;
      }
      for (gl::vertex_id v = 1; v <= n; ++v) {
        if (!used[v] && arc(x, v) != inf) {
          used[v] = true;
          walk(v, sum + arc(x, v));
          used[v] = false;
        }
      }
    };
    used[u] = true;
    walk(u, 0.0);
    return best;
  }
};
} // namespace

TEST_CASE("bellman_ford differential: brute force on tiny graphs")
{
  std::mt19937 rng(4242);
  for (int trial = 0; trial < 300; ++trial) {
    const gl::vertex_id n = 2 + rng() % 5; // 2..6
    const std::size_t m = rng() % (3 * n);
    const auto arcs = random_arcs(
      rng, n, m, [&] { return static_cast<double>(static_cast<int>(rng() % 9) - 3); });
    CAPTURE(trial);
    const auto g = directed_csr::from_edges(n, arcs);
    const brute ref{ n, lightest(n, arcs) };
    const auto on_cycle = ref.on_negative_cycle();

    const bool any = std::ranges::any_of(on_cycle, [](bool b) { return b; });
    CHECK(find_negative_cycle(g).found == any);

    for (gl::vertex_id t = 1; t <= n; ++t) {
      const auto to = bellman_ford(g, t);
      for (gl::vertex_id v = 1; v <= n; ++v) {
        CAPTURE(t);
        CAPTURE(v);
        bool minus_inf = false;
        for (gl::vertex_id c = 1; c <= n; ++c) {
          if (on_cycle[c] && ref.reaches(v, c) && ref.reaches(c, t)) {
            minus_inf = true;
          }
        }
        if (minus_inf) {
          CHECK(to.dist[v] == -inf);
        } else if (v == t) {
          CHECK(to.dist[v] == 0.0);
        } else {
          CHECK(to.dist[v] == ref.simple_path(v, t));
        }
      }
    }
  }
}

TEST_CASE("bellman_ford differential: the three variants")
{
  // same distances; stop_when_stable and frontier take the same rounds;
  // textbook >= stop_when_stable >= frontier in rounds and relaxations
  std::mt19937 rng(31337);
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

    for (gl::vertex_id t = 1; t <= std::min<gl::vertex_id>(n, 5); ++t) {
      CAPTURE(t);
      bellman_ford_stats textbook;
      bellman_ford_stats stop;
      bellman_ford_stats frontier;
      const auto a = bellman_ford(g, t, {}, { bellman_ford_variant::textbook }, &textbook);
      const auto b =
        bellman_ford(g, t, {}, { bellman_ford_variant::stop_when_stable }, &stop);
      const auto c = bellman_ford(g, t, {}, { bellman_ford_variant::frontier }, &frontier);
      CHECK(a.dist == b.dist);
      CHECK(b.dist == c.dist);
      CHECK(stop.rounds == frontier.rounds);
      CHECK(textbook.rounds >= stop.rounds);
      CHECK(textbook.relaxations >= stop.relaxations);
      CHECK(stop.relaxations >= frontier.relaxations);
    }
  }
}
