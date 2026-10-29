// Behaviour every Dijkstra version must have. Write each case once and run it
// for all three versions with TEST_CASE_TEMPLATE over small tag structs, e.g.
//
//   struct array_version { template<typename G> static auto run(const G& g, gl::vertex_id s)
//                          { return gl::algorithms::dijkstra_array(g, s); } };
//
// (same for heap and lazy). Remove doctest::skip() from a case once it is real.
// Note: a case may only call a version once its body exists, or linking fails.

#include "graph/algorithms/dijkstra.hpp"
#include "graph/graph.hpp"

#include "doctest/doctest.h"

TEST_CASE("dijkstra: single vertex" * doctest::skip())
{
  // n = 1: dist[1] == 0, parent[1] == 0, source == 1.
}

TEST_CASE("dijkstra: hand-checked small graph" * doctest::skip())
{
  // A ~6-vertex graph where the shortest path is NOT the one with fewest
  // edges (e.g. 1-2 weight 10, 1-3-2 weights 1 + 1). Check every dist and
  // parent against values worked out on paper.
}

TEST_CASE("dijkstra: unreachable vertices" * doctest::skip())
{
  // Two components: the other one has dist == +inf
  // (std::numeric_limits<W>::infinity()) and parent == 0.
}

TEST_CASE("dijkstra: zero-weight edges" * doctest::skip())
{
  // Weight 0 is allowed (it is not negative): dist ties must not break parent.
}

TEST_CASE("dijkstra: negative weight throws" * doctest::skip())
{
  // Any edge < 0 -> CHECK_THROWS_AS(..., std::invalid_argument), for every
  // version and both storages, before any work is done.
}

TEST_CASE("dijkstra: CSR and matrix give the same result" * doctest::skip())
{
  // Same edges in csr_graph and matrix_graph -> identical dist and parent.
}

TEST_CASE("dijkstra: float weights" * doctest::skip())
{
  // weighted<float> compiles and gives float dist (doctest::Approx).
}
