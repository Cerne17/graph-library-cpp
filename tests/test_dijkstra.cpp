// Behaviour every Dijkstra version must have. Each case is written once and
// runs for all three versions through TEST_CASE_TEMPLATE over the tag structs
// below. Expected values are worked out by hand.

#include "graph/algorithms/dijkstra.hpp"
#include "graph/algorithms/path.hpp"
#include "graph/graph.hpp"
#include "graph/policies.hpp"
#include "graph/types.hpp"

#include "doctest/doctest.h"

#include <concepts>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
struct array_version
{
  template<typename G>
  static auto run(const G& g, gl::vertex_id s)
  {
    return gl::algorithms::dijkstra_array(g, s);
  }
};

struct heap_version
{
  template<typename G>
  static auto run(const G& g, gl::vertex_id s)
  {
    return gl::algorithms::dijkstra_heap(g, s);
  }
};

struct lazy_version
{
  template<typename G>
  static auto run(const G& g, gl::vertex_id s)
  {
    return gl::algorithms::dijkstra_lazy(g, s);
  }
};

template<typename W = double>
using undirected_csr = gl::csr_graph<gl::undirected, gl::weighted<W>>;
template<typename W = double>
using directed_csr = gl::csr_graph<gl::directed, gl::weighted<W>>;
using undirected_matrix = gl::matrix_graph<gl::undirected, gl::weighted<>>;
using directed_matrix = gl::matrix_graph<gl::directed, gl::weighted<>>;
using wedge = gl::weighted_edge<double>;

constexpr double inf = std::numeric_limits<double>::infinity();
} // namespace

#define DIJKSTRA_VERSIONS array_version, heap_version, lazy_version

TEST_CASE_TEMPLATE("dijkstra: single vertex", V, DIJKSTRA_VERSIONS)
{
  const auto g = undirected_csr<>::from_edges(1, {});
  const auto sp = V::run(g, 1);

  CHECK(sp.source == 1);
  REQUIRE(sp.dist.size() == 2);
  REQUIRE(sp.parent.size() == 2);
  CHECK(sp.dist[1] == 0.0);
  CHECK(sp.parent[1] == 0);
}

TEST_CASE_TEMPLATE("dijkstra: hand-checked small graph", V, DIJKSTRA_VERSIONS)
{
  SUBCASE("statement example: 1-3-2 beats the direct 1-2")
  {
    // 1-2 (10), 1-3 (1), 3-2 (1), 4 isolated
    const std::vector<wedge> edges{ { 1, 2, 10.0 }, { 1, 3, 1.0 }, { 3, 2, 1.0 } };
    const auto g = undirected_csr<>::from_edges(4, edges);
    const auto sp = V::run(g, 1);

    CHECK(sp.dist == std::vector<double>{ inf, 0.0, 2.0, 1.0, inf });
    CHECK(sp.parent == std::vector<gl::vertex_id>{ 0, 0, 3, 1, 0 });
    CHECK(gl::algorithms::reconstruct_path(sp, 2) ==
          std::vector<gl::vertex_id>{ 1, 3, 2 });
    CHECK(gl::algorithms::reconstruct_path(sp, 1) == std::vector<gl::vertex_id>{ 1 });
    CHECK(gl::algorithms::reconstruct_path(sp, 4).empty());
  }

  SUBCASE("six vertices, the shortest path is not the one with fewest edges")
  {
    //   1-2 (7)  1-3 (9)  1-6 (14)  2-3 (10)  2-4 (15)
    //   3-4 (11) 3-6 (2)  4-5 (6)   5-6 (9)
    // from 1: 2 = 7 (1), 3 = 9 (1), 6 = 9 + 2 = 11 (3),
    //         4 = 9 + 11 = 20 (3), 5 = 11 + 9 = 20 (6)
    const std::vector<wedge> edges{ { 1, 2, 7.0 },  { 1, 3, 9.0 }, { 1, 6, 14.0 },
                                    { 2, 3, 10.0 }, { 2, 4, 15.0 }, { 3, 4, 11.0 },
                                    { 3, 6, 2.0 },  { 4, 5, 6.0 },  { 5, 6, 9.0 } };
    const auto g = undirected_csr<>::from_edges(6, edges);
    const auto sp = V::run(g, 1);

    CHECK(sp.dist ==
          std::vector<double>{ inf, 0.0, 7.0, 9.0, 20.0, 20.0, 11.0 });
    CHECK(sp.parent == std::vector<gl::vertex_id>{ 0, 0, 1, 1, 3, 6, 3 });
    CHECK(gl::algorithms::reconstruct_path(sp, 5) ==
          std::vector<gl::vertex_id>{ 1, 3, 6, 5 });
  }

  SUBCASE("directed: edges are one-way")
  {
    // 1->2 (1), 2->3 (1), 3->1 (1), 1->3 (5); from 2: 3 = 1, 1 = 2
    const std::vector<wedge> edges{ { 1, 2, 1.0 }, { 2, 3, 1.0 }, { 3, 1, 1.0 },
                                    { 1, 3, 5.0 } };
    const auto g = directed_csr<>::from_edges(3, edges);
    const auto sp = V::run(g, 2);

    CHECK(sp.dist == std::vector<double>{ inf, 2.0, 0.0, 1.0 });
    CHECK(sp.parent == std::vector<gl::vertex_id>{ 0, 3, 0, 2 });
  }
}

TEST_CASE_TEMPLATE("dijkstra: unreachable vertices", V, DIJKSTRA_VERSIONS)
{
  SUBCASE("two components")
  {
    // {1, 2, 3} and {4, 5}
    const std::vector<wedge> edges{ { 1, 2, 1.0 }, { 2, 3, 2.0 }, { 4, 5, 1.0 } };
    const auto g = undirected_csr<>::from_edges(5, edges);
    const auto sp = V::run(g, 2);

    CHECK(sp.dist == std::vector<double>{ inf, 1.0, 0.0, 2.0, inf, inf });
    CHECK(sp.parent == std::vector<gl::vertex_id>{ 0, 2, 0, 2, 0, 0 });
  }

  SUBCASE("directed: reachable only against the edge direction")
  {
    // 2->1 only: 1 cannot reach 2
    const std::vector<wedge> edges{ { 2, 1, 1.0 } };
    const auto g = directed_csr<>::from_edges(2, edges);
    const auto sp = V::run(g, 1);

    CHECK(sp.dist[2] == inf);
    CHECK(sp.parent[2] == 0);
  }
}

TEST_CASE_TEMPLATE("dijkstra: zero-weight edges", V, DIJKSTRA_VERSIONS)
{
  // 1-2 (0), 2-3 (0), 1-3 (0), 3-4 (2)
  // 2 and 3 are both at 0 straight from 1; a tie must not replace the parent
  const std::vector<wedge> edges{ { 1, 2, 0.0 }, { 2, 3, 0.0 }, { 1, 3, 0.0 },
                                  { 3, 4, 2.0 } };
  const auto g = undirected_csr<>::from_edges(4, edges);
  const auto sp = V::run(g, 1);

  CHECK(sp.dist == std::vector<double>{ inf, 0.0, 0.0, 0.0, 2.0 });
  CHECK(sp.parent == std::vector<gl::vertex_id>{ 0, 0, 1, 1, 3 });
}

TEST_CASE_TEMPLATE("dijkstra: negative weight throws", V, DIJKSTRA_VERSIONS)
{
  // the negative edge is unreachable from the source: the check must still
  // reject the graph up front, not while relaxing
  const std::vector<wedge> edges{ { 1, 2, 1.0 }, { 3, 4, -0.5 } };

  CHECK_THROWS_AS(V::run(undirected_csr<>::from_edges(4, edges), 1),
                  std::invalid_argument);
  CHECK_THROWS_AS(V::run(directed_csr<>::from_edges(4, edges), 1),
                  std::invalid_argument);
  CHECK_THROWS_AS(V::run(undirected_matrix::from_edges(4, edges), 1),
                  std::invalid_argument);
  CHECK_THROWS_AS(V::run(directed_matrix::from_edges(4, edges), 1),
                  std::invalid_argument);
}

TEST_CASE_TEMPLATE("dijkstra: CSR and matrix give the same result", V, DIJKSTRA_VERSIONS)
{
  // the six-vertex graph above, plus an isolated vertex 7
  const std::vector<wedge> edges{ { 1, 2, 7.0 },  { 1, 3, 9.0 }, { 1, 6, 14.0 },
                                  { 2, 3, 10.0 }, { 2, 4, 15.0 }, { 3, 4, 11.0 },
                                  { 3, 6, 2.0 },  { 4, 5, 6.0 },  { 5, 6, 9.0 } };

  SUBCASE("undirected")
  {
    const auto csr = undirected_csr<>::from_edges(7, edges);
    const auto matrix = undirected_matrix::from_edges(7, edges);
    for (gl::vertex_id s = 1; s <= 7; ++s) {
      CAPTURE(s);
      const auto a = V::run(csr, s);
      const auto b = V::run(matrix, s);
      CHECK(a.dist == b.dist);
      CHECK(a.parent == b.parent);
    }
  }

  SUBCASE("directed")
  {
    const auto csr = directed_csr<>::from_edges(7, edges);
    const auto matrix = directed_matrix::from_edges(7, edges);
    for (gl::vertex_id s = 1; s <= 7; ++s) {
      CAPTURE(s);
      const auto a = V::run(csr, s);
      const auto b = V::run(matrix, s);
      CHECK(a.dist == b.dist);
      CHECK(a.parent == b.parent);
    }
  }
}

TEST_CASE_TEMPLATE("dijkstra: float weights", V, DIJKSTRA_VERSIONS)
{
  const std::vector<gl::weighted_edge<float>> edges{ { 1, 2, 10.0f },
                                                     { 1, 3, 0.25f },
                                                     { 3, 2, 1.5f } };
  const auto g = undirected_csr<float>::from_edges(4, edges);
  const auto sp = V::run(g, 1);

  static_assert(std::same_as<typename decltype(sp.dist)::value_type, float>);
  CHECK(sp.dist[1] == 0.0f);
  CHECK(sp.dist[2] == doctest::Approx(1.75));
  CHECK(sp.dist[3] == doctest::Approx(0.25));
  CHECK(sp.dist[4] == std::numeric_limits<float>::infinity());
  CHECK(sp.parent == std::vector<gl::vertex_id>{ 0, 0, 3, 1, 0 });
}
