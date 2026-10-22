#pragma once

#include "graph/policies.hpp"
#include "graph/types.hpp"
#include <cassert>
#include <span>
#include <vector>

namespace gl {
template<direction_policy D, weight_policy W>
class csr_storage
{
  static_assert(!W::is_weighted,
                "csr_storage: weighted graphs not supported yet");

private:
  csr_storage() = default;

  vertex_id n_ = 0;
  std::vector<edge_index> offsets_;
  std::vector<vertex_id> targets_;

public:
  using direction_type = D;
  using weighting_type = W;

  // takes the total amount of vertices and an edge collection.
  // Builds offsets_ and targets_.
  // The only way to build a csr_storage, since the constructor is private.
  static csr_storage from_edges(vertex_id n, std::span<const edge> edges);

  vertex_id vertex_count() const { return n_; }

  // Visits each neighbor of the passed vertex.
  // The lambda function tells it how to behave when visiting a neighbor.
  template<typename F>
  void for_each_neighbor(vertex_id u, F&& f) const;

  std::span<const edge_index> offsets() const
  {
    return offsets_;
  }; // non-owning read only contiguous space of memory
  std::span<const vertex_id> targets() const { return targets_; };
};

template<direction_policy D, weight_policy W>
csr_storage<D, W> csr_storage<D, W>::from_edges(vertex_id n,
                                                std::span<const edge> edges)
{
  csr_storage s;
  s.n_ = n;

  // 1. count edges
  std::vector<edge_index> deg(n + 1, 0);
  for (const edge& e : edges) {
    assert(e.u >= 1 && e.u <= n && "from_edges: u out of range");
    assert(e.v >= 1 && e.v <= n && "from_edges: v out of range");
    assert(e.u != e.v && "from_edges: self-loops not supported");

    ++deg[e.u];
    if constexpr (!D::is_directed) {
      ++deg[e.v];
    }
  }

  // 2. prefix sum
  s.offsets_.resize(n + 2, 0);
  for (vertex_id u = 1; u <= n; ++u) {
    s.offsets_[u + 1] = s.offsets_[u] + deg[u];
  }

  // 3. fill: cursor[u] = next free slot in u's block
  s.targets_.resize(s.offsets_[n + 1]);
  std::vector<edge_index> cursor = s.offsets_;
  for (const edge& e : edges) {
    s.targets_[cursor[e.u]++] = e.v;
    if constexpr (!D::is_directed) {
      s.targets_[cursor[e.v]++] = e.u;
    }
  }

  // 4. sort each block: ascending neighbor order
  for (vertex_id u = 1; u <= n; ++u) {
    std::sort(s.targets_.begin() + s.offsets_[u],
              s.targets_.begin() + s.offsets_[u + 1]);
  }

  // 5. Returning the object
  // The only valid way to spawn a csr
  return s;
}

template<direction_policy D, weight_policy W>
template<typename F>
void csr_storage<D, W>::for_each_neighbor(vertex_id u, F&& f) const
{
  assert(u >= 1 && u <= n_ && "for_each_neighbor: u out of range");
  for (edge_index i = offsets_[u]; i < offsets_[u + 1]; ++i) {
    f(targets_[i]);
  }
}
} // namespace gl
