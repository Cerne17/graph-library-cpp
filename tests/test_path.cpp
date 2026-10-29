// reconstruct_path over both kinds of tree, built by hand (no algorithm runs).

#include "graph/algorithms/path.hpp"
#include "graph/algorithms/search.hpp"
#include "graph/algorithms/shortest_paths.hpp"
#include "graph/types.hpp"

#include "doctest/doctest.h"

#include <limits>
#include <vector>

static_assert(gl::algorithms::parent_tree<gl::algorithms::search_tree>);
static_assert(gl::algorithms::parent_tree<gl::algorithms::shortest_paths<double>>);
static_assert(gl::algorithms::parent_tree<gl::algorithms::shortest_paths<float>>);

namespace {
struct no_source // has a parent array but no source
{
  std::vector<gl::vertex_id> parent;
};

struct no_parent // has a source but no parent array
{
  gl::vertex_id source = 0;
};
} // namespace

static_assert(!gl::algorithms::parent_tree<no_source>);
static_assert(!gl::algorithms::parent_tree<no_parent>);

namespace {
using vertex_path = std::vector<gl::vertex_id>;

// Tree rooted at 3 over vertices 1..6:
//   3 -> 1 -> 5 -> 2,  3 -> 4,  6 unreached.
gl::algorithms::search_tree hand_search_tree()
{
  return { 3, { 0, 3, 5, 0, 3, 1, 0 }, { -1, 1, 3, 0, 1, 2, -1 } };
}

gl::algorithms::shortest_paths<double> hand_shortest_paths()
{
  constexpr double inf = std::numeric_limits<double>::infinity();
  return { 3,
           { inf, 1.0, 3.5, 0.0, 2.0, 2.5, inf },
           { 0, 3, 5, 0, 3, 1, 0 } };
}
} // namespace

TEST_CASE("reconstruct_path: path to the source is {source}")
{
  CHECK(gl::algorithms::reconstruct_path(hand_search_tree(), 3) == vertex_path{ 3 });
  CHECK(gl::algorithms::reconstruct_path(hand_shortest_paths(), 3) ==
        vertex_path{ 3 });

  const gl::algorithms::search_tree single{ 1, { 0, 0 }, { -1, 0 } };
  CHECK(gl::algorithms::reconstruct_path(single, 1) == vertex_path{ 1 });
}

TEST_CASE("reconstruct_path: hand-built tree gives source..target in order")
{
  const auto tree = hand_search_tree();
  CHECK(gl::algorithms::reconstruct_path(tree, 1) == vertex_path{ 3, 1 });
  CHECK(gl::algorithms::reconstruct_path(tree, 5) == vertex_path{ 3, 1, 5 });
  CHECK(gl::algorithms::reconstruct_path(tree, 2) == vertex_path{ 3, 1, 5, 2 });
  CHECK(gl::algorithms::reconstruct_path(tree, 4) == vertex_path{ 3, 4 });
}

TEST_CASE("reconstruct_path: unreachable target gives an empty path")
{
  CHECK(gl::algorithms::reconstruct_path(hand_search_tree(), 6).empty());
  CHECK(gl::algorithms::reconstruct_path(hand_shortest_paths(), 6).empty());
}

TEST_CASE("reconstruct_path: works with search_tree and shortest_paths")
{
  // same parent array in both trees -> same paths
  const auto bfs_tree = hand_search_tree();
  const auto sp_tree = hand_shortest_paths();
  for (gl::vertex_id v = 1; v <= 6; ++v) {
    CAPTURE(v);
    CHECK(gl::algorithms::reconstruct_path(bfs_tree, v) ==
          gl::algorithms::reconstruct_path(sp_tree, v));
  }
}
