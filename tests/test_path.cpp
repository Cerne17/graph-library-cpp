// reconstruct_path over both kinds of tree. Remove doctest::skip() once real.

#include "graph/algorithms/path.hpp"
#include "graph/algorithms/search.hpp"
#include "graph/algorithms/shortest_paths.hpp"

#include "doctest/doctest.h"

TEST_CASE("reconstruct_path: path to the source is {source}" * doctest::skip())
{
}

TEST_CASE("reconstruct_path: hand-built tree gives source..target in order" *
          doctest::skip())
{
  // Build a search_tree / shortest_paths by hand (no algorithm needed).
}

TEST_CASE("reconstruct_path: unreachable target gives an empty path" *
          doctest::skip())
{
}

TEST_CASE("reconstruct_path: works with search_tree and shortest_paths" *
          doctest::skip())
{
  // static_assert(gl::algorithms::parent_tree<gl::algorithms::search_tree>);
  // static_assert(gl::algorithms::parent_tree<gl::algorithms::shortest_paths<double>>);
}
