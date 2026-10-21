#include "graph/concepts.hpp"
#include "graph/policies.hpp"
#include "graph/storage/csr.hpp"

static_assert(gl::graph_like<gl::csr_storage<gl::undirected, gl::unweighted>>);
