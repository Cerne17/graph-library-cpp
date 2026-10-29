#include "doctest/doctest.h"
#include "graph/concepts.hpp"
#include "graph/graph.hpp"
#include "graph/policies.hpp"
#include "graph/storage/csr.hpp"
#include "graph/storage/matrix.hpp"

#include <concepts>

// each alias names exactly its storage type
static_assert(std::same_as<gl::csr_graph<gl::undirected, gl::unweighted>,
                           gl::csr_storage<gl::undirected, gl::unweighted>>);
static_assert(std::same_as<gl::csr_graph<gl::directed, gl::weighted<>>,
                           gl::csr_storage<gl::directed, gl::weighted<>>>);
static_assert(
  std::same_as<gl::matrix_graph<gl::undirected, gl::unweighted>,
               gl::matrix_storage<gl::undirected, gl::unweighted>>);
static_assert(std::same_as<gl::matrix_graph<gl::directed, gl::weighted<float>>,
                           gl::matrix_storage<gl::directed, gl::weighted<float>>>);

static_assert(gl::graph_like<gl::csr_graph<gl::undirected, gl::unweighted>>);
static_assert(gl::graph_like<gl::matrix_graph<gl::directed, gl::unweighted>>);
static_assert(
  !gl::weighted_graph<gl::csr_graph<gl::undirected, gl::unweighted>>);
static_assert(
  !gl::weighted_graph<gl::matrix_graph<gl::undirected, gl::unweighted>>);
static_assert(gl::weighted_graph<gl::csr_graph<gl::undirected, gl::weighted<>>>);
static_assert(gl::weighted_graph<gl::matrix_graph<gl::directed, gl::weighted<>>>);
