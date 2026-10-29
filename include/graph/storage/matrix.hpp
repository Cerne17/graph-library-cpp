#pragma once

#include "graph/policies.hpp"
#include "graph/storage/input_edge.hpp"
#include "graph/storage/weight_column.hpp"
#include "graph/types.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace gl {
namespace detail {

// General version (unweighted): one bit per cell, rows packed in 64-bit words.
// Bit v of row u is set when the edge (u, v) exists. Bit 0 is unused, since
// vertices are 1-indexed.
template<weight_policy W, bool = W::is_weighted>
class matrix_cells
{
private:
  static constexpr std::size_t bits_per_word = 64;

  std::size_t words_per_row_;
  std::vector<std::uint64_t> words_; // row u starts at u * words_per_row_

public:
  // n + 1 bits per row (bits 0..n), rounded up to whole words
  explicit matrix_cells(vertex_id n)
    : words_per_row_((static_cast<std::size_t>(n) + bits_per_word) /
                     bits_per_word)
    , words_((static_cast<std::size_t>(n) + 1) * words_per_row_, 0)
  {
  }

  void set(vertex_id u, vertex_id v)
  {
    words_[u * words_per_row_ + v / bits_per_word] |= std::uint64_t{ 1 }
                                                      << (v % bits_per_word);
  }

  bool test(vertex_id u, vertex_id v) const
  {
    return (words_[u * words_per_row_ + v / bits_per_word] >>
            (v % bits_per_word)) &
           1;
  }

  // Calls f(v) for every set bit v in row u, in ascending order.
  template<typename F>
  void for_each_in_row(vertex_id u, F&& f) const;

  std::size_t bytes() const { return words_.size() * sizeof(std::uint64_t); }
};

template<weight_policy W, bool B>
template<typename F>
void matrix_cells<W, B>::for_each_in_row(vertex_id u, F&& f) const
{
  const std::size_t row = u * words_per_row_;
  for (std::size_t i = 0; i < words_per_row_; ++i) {
    std::uint64_t word = words_[row + i];
    while (word != 0) {
      const auto bit = static_cast<std::size_t>(std::countr_zero(word));
      f(static_cast<vertex_id>(i * bits_per_word + bit));
      word &= word - 1; // clear the lowest set bit
    }
  }
}

// Specialization (weighted): one weight per cell, +inf meaning "no edge".
template<weight_policy W>
class matrix_cells<W, true>
{
public:
  using weight_type = typename W::weight_type;

private:
  static constexpr weight_type no_edge =
    std::numeric_limits<weight_type>::infinity();

  vertex_id n_;
  std::vector<weight_type> cells_; // cell (u, v) at u * (n_ + 1) + v

  std::size_t index(vertex_id u, vertex_id v) const
  {
    return u * (static_cast<std::size_t>(n_) + 1) + v;
  }

public:
  bool has_negative = false;

  explicit matrix_cells(vertex_id n)
    : n_(n)
    , cells_((static_cast<std::size_t>(n) + 1) *
               (static_cast<std::size_t>(n) + 1),
             no_edge)
  {
  }

  // Parallel edges keep the lightest weight.
  void set(vertex_id u, vertex_id v, weight_type w)
  {
    weight_type& cell = cells_[index(u, v)];
    cell = std::min(cell, w);
    if (w < 0) {
      has_negative = true;
    }
  }

  bool test(vertex_id u, vertex_id v) const
  {
    return cells_[index(u, v)] != no_edge;
  }

  // Calls f(v, w) for every edge (u, v) of weight w, in ascending v.
  template<typename F>
  void for_each_in_row(vertex_id u, F&& f) const;

  std::size_t bytes() const { return cells_.size() * sizeof(weight_type); }
};

template<weight_policy W>
template<typename F>
void matrix_cells<W, true>::for_each_in_row(vertex_id u, F&& f) const
{
  for (vertex_id v = 1; v <= n_; ++v) {
    const weight_type w = cells_[index(u, v)];
    if (w != no_edge) {
      f(v, w);
    }
  }
}

} // namespace detail

template<direction_policy D, weight_policy W>
class matrix_storage : public weight_type_member<W>
{
public:
  using input_edge = input_edge_t<W>;

private:
  explicit matrix_storage(vertex_id n)
    : n_(n)
    , cells_(n)
  {
  }

  vertex_id n_;
  detail::matrix_cells<W> cells_;

public:
  using direction_type = D;
  using weighting_type = W;

  // The only way to build a matrix_storage, since the constructor is private.
  static matrix_storage from_edges(vertex_id n,
                                   std::span<const input_edge> edges);

  vertex_id vertex_count() const { return n_; }

  // Visits each neighbor of u, in ascending order.
  template<typename F>
  void for_each_neighbor(vertex_id u, F&& f) const;

  bool has_edge(vertex_id u, vertex_id v) const
  {
    assert(u >= 1 && u <= n_ && "has_edge: u out of range");
    assert(v >= 1 && v <= n_ && "has_edge: v out of range");
    return cells_.test(u, v);
  }

  bool has_negative_weight() const
    requires W::is_weighted
  {
    return cells_.has_negative;
  }

  // Visits each (neighbor, weight) pair of u, in ascending neighbor order.
  template<typename F>
  void for_each_edge(vertex_id u, F&& f) const
    requires W::is_weighted;
};

template<direction_policy D, weight_policy W>
matrix_storage<D, W> matrix_storage<D, W>::from_edges(
  vertex_id n,
  std::span<const input_edge> edges)
{
  matrix_storage s(n);

  for (const input_edge& e : edges) {
    assert(e.u >= 1 && e.u <= n && "from_edges: u out of range");
    assert(e.v >= 1 && e.v <= n && "from_edges: v out of range");
    assert(e.u != e.v && "from_edges: self-loops not supported");

    if constexpr (W::is_weighted) {
      s.cells_.set(e.u, e.v, e.weight);
      if constexpr (!D::is_directed) {
        s.cells_.set(e.v, e.u, e.weight);
      }
    } else {
      s.cells_.set(e.u, e.v);
      if constexpr (!D::is_directed) {
        s.cells_.set(e.v, e.u);
      }
    }
  }

  return s;
}

template<direction_policy D, weight_policy W>
template<typename F>
void matrix_storage<D, W>::for_each_neighbor(vertex_id u, F&& f) const
{
  assert(u >= 1 && u <= n_ && "for_each_neighbor: u out of range");
  if constexpr (W::is_weighted) {
    cells_.for_each_in_row(u, [&](vertex_id v, const auto&) { f(v); });
  } else {
    cells_.for_each_in_row(u, f);
  }
}

template<direction_policy D, weight_policy W>
template<typename F>
void matrix_storage<D, W>::for_each_edge(vertex_id u, F&& f) const
  requires W::is_weighted
{
  assert(u >= 1 && u <= n_ && "for_each_edge: u out of range");
  cells_.for_each_in_row(u, f);
}
} // namespace gl
