// Cross-checks on random graphs (fixed seeds, several sizes and densities).
// Remove doctest::skip() from a case once it is real.

#include "graph/algorithms/dijkstra.hpp"
#include "graph/algorithms/search.hpp"
#include "graph/graph.hpp"

#include "doctest/doctest.h"

TEST_CASE("dijkstra: the three versions agree on dist" * doctest::skip())
{
  // dist must match exactly (doctest::Approx for floats). parent may differ
  // when there are ties, so compare it through the next case instead.
}

TEST_CASE("dijkstra: parent is a valid shortest-path tree" * doctest::skip())
{
  // For every reachable v != source: dist[v] == dist[parent[v]] + w(parent[v], v).
}

TEST_CASE("dijkstra: unit weights reproduce BFS levels" * doctest::skip())
{
  // All weights 1.0 -> dist[v] == bfs(g, s).level[v] (and +inf where level == -1).
}
