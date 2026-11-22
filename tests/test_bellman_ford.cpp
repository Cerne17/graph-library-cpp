// Bellman-Ford toward a target. Every expected value is worked out by hand
// from the arcs written next to it.

#include "graph/algorithms/bellman_ford.hpp"

#include "graph/algorithms/path.hpp"
#include "graph/graph.hpp"
#include "graph/policies.hpp"
#include "graph/storage/reversed.hpp"
#include "graph/types.hpp"

#include "doctest/doctest.h"

#include <concepts>
#include <limits>
#include <vector>

namespace {
using gl::algorithms::bellman_ford;
using gl::algorithms::bellman_ford_stats;
using gl::algorithms::bellman_ford_variant;
using gl::algorithms::path_to_target;
using gl::algorithms::target_paths;

using wedge = gl::weighted_edge<double>;
using ids = std::vector<gl::vertex_id>;
using directed_csr = gl::csr_graph<gl::directed, gl::weighted<>>;
using directed_matrix = gl::matrix_graph<gl::directed, gl::weighted<>>;
using undirected_csr = gl::csr_graph<gl::undirected, gl::weighted<>>;
using unweighted_csr = gl::csr_graph<gl::directed, gl::unweighted>;

constexpr double inf = std::numeric_limits<double>::infinity();

template<typename G>
concept can_bellman_ford = requires(const G& g) { bellman_ford(g, 1); };

template<typename R>
concept can_run_on_reversed = requires(const R& r) {
  gl::algorithms::bellman_ford_on_reversed(r, 1);
};

template<typename W>
concept target_paths_type = requires { typename target_paths<W>; };
} // namespace

static_assert(can_bellman_ford<directed_csr>);
static_assert(can_bellman_ford<undirected_csr>);
static_assert(!can_bellman_ford<unweighted_csr>);
static_assert(!can_bellman_ford<gl::reversed_graph<directed_csr>>);
static_assert(can_run_on_reversed<gl::reversed_graph<directed_csr>>);
static_assert(can_run_on_reversed<undirected_csr>);
static_assert(!can_run_on_reversed<directed_csr>); // not reversed
static_assert(!gl::algorithms::parent_tree<target_paths<double>>);
static_assert(target_paths_type<double>);
static_assert(!target_paths_type<int>);

TEST_CASE_TEMPLATE("bellman_ford: negative arcs, no negative cycle",
                   G,
                   directed_csr,
                   directed_matrix)
{
  // 1->2 (4), 1->3 (2), 3->2 (-1), 2->4 (3), 3->4 (6), 4->5 (1), target 4:
  // 2: 3; 3: min(6, -1 + 3) = 2 via 2; 1: min(4 + 3, 2 + 2) = 4 via 3;
  // 5 has no way to 4
  const std::vector<wedge> edges{ { 1, 2, 4.0 },  { 1, 3, 2.0 }, { 3, 2, -1.0 },
                                  { 2, 4, 3.0 },  { 3, 4, 6.0 }, { 4, 5, 1.0 } };
  const auto g = G::from_edges(5, edges);
  const auto t = bellman_ford(g, 4);

  CHECK(t.target == 4);
  CHECK(t.dist == std::vector<double>{ inf, 4.0, 3.0, 2.0, 0.0, inf });
  CHECK(t.next == ids{ 0, 3, 4, 2, 0, 0 });
  CHECK_FALSE(t.cycle_reaches_target);
  CHECK(path_to_target(t, 1) == ids{ 1, 3, 2, 4 });
  CHECK(path_to_target(t, 2) == ids{ 2, 4 });
  CHECK(path_to_target(t, 4) == ids{ 4 });
  CHECK(path_to_target(t, 5).empty());
}

TEST_CASE("bellman_ford: single vertex and unreachable vertices")
{
  const auto one = bellman_ford(directed_csr::from_edges(1, {}), 1);
  CHECK(one.dist == std::vector<double>{ inf, 0.0 });
  CHECK(one.next == ids{ 0, 0 });

  // 1->2 (1), 3->4 (1), target 2
  const std::vector<wedge> edges{ { 1, 2, 1.0 }, { 3, 4, 1.0 } };
  const auto t = bellman_ford(directed_csr::from_edges(4, edges), 2);
  CHECK(t.dist == std::vector<double>{ inf, 1.0, 0.0, inf, inf });
  CHECK(t.next == ids{ 0, 2, 0, 0, 0 });
}

TEST_CASE("bellman_ford: undirected graph, no reversal needed")
{
  // n = 4 with vertex 4 isolated: 1-2 (10), 1-3 (1), 3-2 (1), target 2
  const std::vector<wedge> edges{ { 1, 2, 10.0 }, { 1, 3, 1.0 }, { 3, 2, 1.0 } };
  const auto t = bellman_ford(undirected_csr::from_edges(4, edges), 2);
  CHECK(t.dist == std::vector<double>{ inf, 2.0, 0.0, 1.0, inf });
  CHECK(t.next == ids{ 0, 3, 0, 2, 0 });
}

TEST_CASE_TEMPLATE("bellman_ford: a negative cycle reaches the target from some vertices",
                   G,
                   directed_csr,
                   directed_matrix)
{
  // 1->2 (1), 2->3 (-3), 3->2 (1), 3->4 (1), 5->4 (2), target 4: the cycle
  // 2->3->2 weighs -2 and reaches 4, so 1, 2, 3 are -inf; 5 only goes 5->4
  const std::vector<wedge> edges{ { 1, 2, 1.0 }, { 2, 3, -3.0 }, { 3, 2, 1.0 },
                                  { 3, 4, 1.0 }, { 5, 4, 2.0 } };
  const auto t = bellman_ford(G::from_edges(5, edges), 4);

  CHECK(t.cycle_reaches_target);
  CHECK(t.dist == std::vector<double>{ inf, -inf, -inf, -inf, 0.0, 2.0 });
  CHECK(t.next == ids{ 0, 0, 0, 0, 0, 4 });
  CHECK(path_to_target(t, 1).empty());
  CHECK(path_to_target(t, 5) == ids{ 5, 4 });
}

TEST_CASE("bellman_ford: a negative cycle that cannot reach the target")
{
  // 1->2 (1), 2->1 (-3), 3->4 (2), 4->1 (1), target 4: the cycle 1-2 is
  // reached from 4 but never reaches it
  const std::vector<wedge> edges{ { 1, 2, 1.0 }, { 2, 1, -3.0 }, { 3, 4, 2.0 },
                                  { 4, 1, 1.0 } };
  const auto t = bellman_ford(directed_csr::from_edges(4, edges), 4);
  CHECK_FALSE(t.cycle_reaches_target);
  CHECK(t.dist == std::vector<double>{ inf, inf, inf, 2.0, 0.0 });
}

TEST_CASE("bellman_ford: a negative cycle through all n vertices")
{
  // ring 1->2->...->6->1, weights 1, 1, 1, 1, 1, -6 (total -1), target 1:
  // found only in round n
  const std::vector<wedge> edges{ { 1, 2, 1.0 }, { 2, 3, 1.0 }, { 3, 4, 1.0 },
                                  { 4, 5, 1.0 }, { 5, 6, 1.0 }, { 6, 1, -6.0 } };
  for (const auto variant : { bellman_ford_variant::textbook,
                              bellman_ford_variant::stop_when_stable,
                              bellman_ford_variant::frontier }) {
    bellman_ford_stats stats;
    const auto t =
      bellman_ford(directed_csr::from_edges(6, edges), 1, {}, { variant }, &stats);
    CHECK(t.cycle_reaches_target);
    CHECK(stats.rounds == 6);
    for (gl::vertex_id v = 1; v <= 6; ++v) {
      CAPTURE(v);
      CHECK(t.dist[v] == -inf);
    }
  }
}

TEST_CASE("bellman_ford: a target on a negative cycle has an empty path")
{
  // 1->2 (-2), 2->1 (1), target 1
  const std::vector<wedge> edges{ { 1, 2, -2.0 }, { 2, 1, 1.0 } };
  const auto t = bellman_ford(directed_csr::from_edges(2, edges), 1);
  CHECK(t.dist[1] == -inf);
  CHECK(path_to_target(t, 1).empty());
}

TEST_CASE("bellman_ford: a zero-weight cycle is not negative")
{
  // 1->2 (0), 2->1 (0), 2->3 (1), target 3
  const std::vector<wedge> edges{ { 1, 2, 0.0 }, { 2, 1, 0.0 }, { 2, 3, 1.0 } };
  const auto t = bellman_ford(directed_csr::from_edges(3, edges), 3);
  CHECK_FALSE(t.cycle_reaches_target);
  CHECK(t.dist == std::vector<double>{ inf, 1.0, 1.0, 0.0 });
}

TEST_CASE("bellman_ford: a negative self-loop alone is a cycle that reaches the target")
{
  // 1->2 (1), 2->3 (1), vertex 4 isolated, plus the loop 2 2 (-1) read from
  // the file, target 3. The rounds stop at round 3, before round n = 4, so
  // they find no cycle; the loop must still make 1 and 2 -inf.
  const std::vector<wedge> edges{ { 1, 2, 1.0 }, { 2, 3, 1.0 } };
  const auto g = directed_csr::from_edges(4, edges);
  const std::vector<wedge> loops{ { 2, 2, -1.0 } };

  const auto plain = bellman_ford(g, 3);
  CHECK(plain.dist == std::vector<double>{ inf, 2.0, 1.0, 0.0, inf });

  bellman_ford_stats stats;
  const auto t = bellman_ford(g, 3, loops, {}, &stats);
  CHECK(stats.rounds == 3); // stopped before round n
  CHECK(t.cycle_reaches_target);
  CHECK(t.dist == std::vector<double>{ inf, -inf, -inf, 0.0, inf });

  // a loop on a vertex that cannot reach the target changes nothing
  const std::vector<wedge> far_loop{ { 4, 4, -1.0 } };
  const auto far = bellman_ford(g, 3, far_loop);
  CHECK_FALSE(far.cycle_reaches_target);
  CHECK(far.dist == std::vector<double>{ inf, 2.0, 1.0, 0.0, inf });
}

TEST_CASE("bellman_ford: each optimization prunes work")
{
  // star: 99 arcs i->100 (1), target 100. Round 1 relaxes the 99 reversed
  // arcs out of 100; round 2 finds nothing new.
  std::vector<wedge> edges;
  for (gl::vertex_id i = 1; i <= 99; ++i) {
    edges.push_back({ i, 100, 1.0 });
  }
  const auto g = directed_csr::from_edges(100, edges);

  const auto run = [&](bellman_ford_variant variant) {
    bellman_ford_stats stats;
    const auto t = bellman_ford(g, 100, {}, { variant }, &stats);
    for (gl::vertex_id i = 1; i <= 99; ++i) {
      CHECK(t.dist[i] == 1.0);
    }
    return stats;
  };

  // textbook: all 100 rounds, 99 arcs each
  const auto textbook = run(bellman_ford_variant::textbook);
  CHECK(textbook.rounds == 100);
  CHECK(textbook.relaxations == 9900);
  // optimization 1: stops after round 2, which scans the 99 arcs again
  const auto stop = run(bellman_ford_variant::stop_when_stable);
  CHECK(stop.rounds == 2);
  CHECK(stop.relaxations == 198);
  // optimizations 1 and 2: round 2 scans only the leaves, which have no arcs
  const auto frontier = run(bellman_ford_variant::frontier);
  CHECK(frontier.rounds == 2);
  CHECK(frontier.relaxations == 99);
}

TEST_CASE("bellman_ford: the convenience call equals the call on a reversal")
{
  const std::vector<wedge> edges{ { 1, 2, 4.0 },  { 1, 3, 2.0 }, { 3, 2, -1.0 },
                                  { 2, 4, 3.0 },  { 3, 4, 6.0 }, { 4, 5, 1.0 } };
  const auto g = directed_csr::from_edges(5, edges);
  const auto a = bellman_ford(g, 4);
  const auto b = gl::algorithms::bellman_ford_on_reversed(gl::reversed(g), 4);
  CHECK(a.target == b.target);
  CHECK(a.dist == b.dist);
  CHECK(a.next == b.next);
  CHECK(a.cycle_reaches_target == b.cycle_reaches_target);
}

TEST_CASE("path_to_target: a corrupt tree cannot loop forever")
{
  // next[1] = 2 and next[2] = 1, target 3: the walk never reaches 3
  const target_paths<double> corrupt{ 3, { inf, 1.0, 1.0, 0.0 }, { 0, 2, 1, 0 }, false };
  CHECK(path_to_target(corrupt, 1).empty());
}

TEST_CASE("bellman_ford: float weights")
{
  const std::vector<gl::weighted_edge<float>> edges{ { 1, 2, 0.5f }, { 2, 3, 0.25f } };
  const auto t =
    bellman_ford(gl::csr_graph<gl::directed, gl::weighted<float>>::from_edges(3, edges), 3);
  static_assert(std::same_as<decltype(t.dist)::value_type, float>);
  CHECK(t.dist[1] == doctest::Approx(0.75));
}
