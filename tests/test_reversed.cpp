// reversed_graph and reversed(): every arc turned around. Expected arc lists
// are written by hand from the input arcs.

#include "graph/storage/reversed.hpp"

#include "graph/concepts.hpp"
#include "graph/graph.hpp"
#include "graph/policies.hpp"
#include "graph/types.hpp"

#include "doctest/doctest.h"

#include <algorithm>
#include <concepts>
#include <utility>
#include <vector>

namespace {
using wedge = gl::weighted_edge<double>;
using arcs = std::vector<std::pair<gl::vertex_id, double>>;

template<typename G>
arcs out_of(const G& g, gl::vertex_id u)
{
  arcs out;
  g.for_each_edge(u, [&](gl::vertex_id v, double w) { out.emplace_back(v, w); });
  return out;
}

using directed_csr = gl::csr_graph<gl::directed, gl::weighted<>>;
using directed_matrix = gl::matrix_graph<gl::directed, gl::weighted<>>;
using undirected_csr = gl::csr_graph<gl::undirected, gl::weighted<>>;
using unweighted_directed_csr = gl::csr_graph<gl::directed, gl::unweighted>;
using reversed_csr = gl::reversed_graph<directed_csr>;

template<typename G>
concept can_reverse = requires(const G& g) { gl::reversed(g); };
} // namespace

static_assert(gl::weighted_graph<reversed_csr>);
static_assert(reversed_csr::direction_type::is_directed);
static_assert(std::same_as<reversed_csr::weight_type, double>);
static_assert(gl::is_reversed_graph<reversed_csr>);
static_assert(!gl::is_reversed_graph<directed_csr>);

static_assert(can_reverse<directed_csr>);
static_assert(can_reverse<directed_matrix>);
static_assert(!can_reverse<undirected_csr>);          // its own reversal
static_assert(!can_reverse<unweighted_directed_csr>); // no weights
static_assert(!can_reverse<reversed_csr>);            // reversed(reversed(g))

TEST_CASE_TEMPLATE("reversed: turns every arc around",
                   G,
                   directed_csr,
                   directed_matrix)
{
  // 1->2 (3), 2->3 (4), 1->3 (5), 3->1 (-2); vertex 4 is isolated
  const std::vector<wedge> edges{ { 1, 2, 3.0 },
                                  { 2, 3, 4.0 },
                                  { 1, 3, 5.0 },
                                  { 3, 1, -2.0 } };
  const auto r = gl::reversed(G::from_edges(4, edges));

  REQUIRE(r.vertex_count() == 4);
  CHECK(out_of(r, 1) == arcs{ { 3, -2.0 } });            // from 3->1
  CHECK(out_of(r, 2) == arcs{ { 1, 3.0 } });             // from 1->2
  CHECK(out_of(r, 3) == arcs{ { 1, 5.0 }, { 2, 4.0 } }); // from 1->3, 2->3
  CHECK(out_of(r, 4).empty());
  CHECK(r.has_negative_weight());

  std::vector<gl::vertex_id> neighbors_of_3;
  r.for_each_neighbor(3, [&](gl::vertex_id v) { neighbors_of_3.push_back(v); });
  CHECK(neighbors_of_3 == std::vector<gl::vertex_id>{ 1, 2 });
}

TEST_CASE("reversed: keeps the parallel arcs of a CSR graph")
{
  const std::vector<wedge> edges{ { 1, 2, 3.0 }, { 1, 2, 1.0 } };
  const auto r = gl::reversed(directed_csr::from_edges(2, edges));

  // equal targets may come out in any order: compare them sorted
  auto at_2 = out_of(r, 2);
  std::ranges::sort(at_2);
  CHECK(at_2 == arcs{ { 1, 1.0 }, { 1, 3.0 } });
  CHECK_FALSE(r.has_negative_weight());
}

TEST_CASE("reversed: graph without arcs")
{
  const auto r = gl::reversed(directed_csr::from_edges(3, {}));
  CHECK(r.vertex_count() == 3);
  CHECK(out_of(r, 2).empty());
}
