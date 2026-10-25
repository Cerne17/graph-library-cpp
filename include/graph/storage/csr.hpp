#pragma once

#include "graph/policies.hpp"
#include "graph/storage/input_edge.hpp"
#include "graph/storage/weight_column.hpp"
#include "graph/types.hpp"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <span>
#include <vector>

namespace gl {
template<direction_policy D, weight_policy W>
class csr_storage : public weight_type_member<W>
{
public:
  using input_edge = input_edge_t<W>;

private:
  csr_storage() = default;

  vertex_id n_ = 0;
  std::vector<edge_index> offsets_;
  std::vector<vertex_id> targets_;
  [[no_unique_address]] weight_column<W> weights_;

public:
  using direction_type = D;
  using weighting_type = W;

  // takes the total amount of vertices and an edge collection.
  // Builds offsets_ and targets_.
  // The only way to build a csr_storage, since the constructor is private.
  static csr_storage from_edges(vertex_id n, std::span<const input_edge> edges);

  vertex_id vertex_count() const { return n_; }

  // Visits each neighbor of the passed vertex.
  // The lambda function tells it how to behave when visiting a neighbor.
  template<typename F>
  void for_each_neighbor(vertex_id u, F&& f) const;

  std::span<const edge_index> offsets() const
  {
    return offsets_;
  } // non-owning read only contiguous space of memory
  std::span<const vertex_id> targets() const { return targets_; }

  bool has_negative_weight() const
    requires W::is_weighted
  {
    return this->wegihts_->has_negative;
  }

  auto weights() const
    requires W::is_weighted;

  template<typename F>
  void for_each_edge(vertex_id u, F&& f) const
    requires W::is_weighted;
};

template<direction_policy D, weight_policy W>
csr_storage<D, W> csr_storage<D, W>::from_edges(
  vertex_id n,
  std::span<const input_edge> edges)
{
  csr_storage s;
  s.n_ = n;

  // 1. count degrees
  s.offsets_.assign(n + 2, 0);
  for (const input_edge& e : edges) {
    assert(e.u >= 1 && e.u <= n && "from_edges: u out of range");
    assert(e.v >= 1 && e.v <= n && "from_edges: v out of range");
    assert(e.u != e.v && "from_edges: self-loops not supported");

    ++s.offsets_[e.u + 1];
    if constexpr (!D::is_directed) {
      ++s.offsets_[e.v + 1];
    }

    if constexpr (W::is_weighted) {
      if (e.weight < 0)
        s.weights_.has_negative = true;
    }
  }

  // 2. prefix sum
  for (vertex_id u = 1; u <= n; ++u) {
    s.offsets_[u + 1] = s.offsets_[u] + s.offsets_[u + 1];
  }

  // 3. fill: cursor[u] = next free slot in u's block
  s.targets_.resize(s.offsets_[n + 1]);
  if constexpr (W::is_weighted) {
    s.weights_.values.resize(s.offsets_[n + 1]);
  }

  std::vector<edge_index> cursor = s.offsets_;
  for (const input_edge& e : edges) {
    const edge_index pos = cursor[e.u]++;
    s.targets_[pos] = e.v;

    if constexpr (W::is_weighted) {
      s.weights_.values[pos] = e.weight;
    }

    if constexpr (!D::is_directed) {
      const edge_index rev_pos = cursor[e.v]++;
      s.targets_[rev_pos] = e.u;
      if constexpr (W::is_weighted) {
        s.weights_.values[rev_pos] = e.weight;
      }
    }
  }

  // 4. sort each block: ascending neighbor order
  for (vertex_id u = 1; u <= n; ++u) {
    const edge_index begin = s.offsets_[u];
    const edge_index len = s.offsets_[u + 1] - s.offsets_[u];

    if constexpr (W::is_weighted) {
      auto t = std::span(s.targets_).subspan(begin, len);
      auto w = std::span(s.weights_.values).subspan(begin, len);

      std::ranges::sort(std::views::zip(t, w), {}, [&](const auto& p) {
        return std::get<0>(p);
      });
    } else {
      std::sort(s.targets_.begin() + s.offsets_[u],
                s.targets_.begin() + s.offsets_[u + 1]);
    }
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

template<direction_policy D, weight_policy W>
auto csr_storage<D, W>::weights() const
  requires W::is_weighted
{
  return std::span<const typename W::weight_type>(weights_.values);
}

template<direction_policy D, weight_policy W>
template<typename F>
void csr_storage<D, W>::for_each_edge(vertex_id u, F&& f) const
  requires W::is_weighted
{
  assert(u >= 1 && u <= n_ && "for_each_edge: u out of range");
  for (edge_index i = offsets_[u]; i < offsets_[u + 1]; ++i) {
    f(targets_[i], weights_[i]);
  }
}
} // namespace gl
