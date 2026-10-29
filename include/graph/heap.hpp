#pragma once

#include "graph/types.hpp"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

namespace gl {

// Binary heap over vertices 1..n, with keys stored per vertex and a position
// map, so a vertex's key can be changed in O(log n).
//
// Ordering rule: comp_(a, b) is true when key a belongs closer to the root than
// key b. std::less<K> (the default) gives a min-heap, std::greater<K> a
// max-heap. Note: std::priority_queue uses the opposite convention.
//
// Invariants (checked by validate()):
//   (I1) heap property: no position i > 0 goes before its parent
//   (I2) positions match: pos_[heap_[i]] == i, for every position i
//   (I3) absent vertices: pos_[v] == not_in_heap for every v not in heap_
template<std::copyable K, typename Compare = std::less<K>>
  requires std::strict_weak_order<Compare, const K&, const K&>
class indexed_heap
{
private:
  static constexpr std::size_t not_in_heap =
    std::numeric_limits<std::size_t>::max();

  std::vector<vertex_id> heap_;  // heap_[i]: vertex at position i (0-based)
  std::vector<K> key_;           // key_[v]: current key of vertex v (1-based)
  std::vector<std::size_t> pos_; // pos_[v]: position of v, or not_in_heap
  [[no_unique_address]] Compare comp_; // zero bytes for empty comparators

  static std::size_t parent(std::size_t i) { return (i - 1) / 2; }

  // key of the vertex sitting at position i
  const K& key_at(std::size_t i) const { return key_[heap_[i]]; }

  // true if the vertex at position i belongs closer to the root than the one
  // at position j
  bool before(std::size_t i, std::size_t j) const
  {
    return comp_(key_at(i), key_at(j));
  }

  // Swaps the vertices at positions i and j, and keeps pos_ in sync.
  // Every move inside the heap goes through here.
  void swap_nodes(std::size_t i, std::size_t j);

  // Moves the vertex at position i up while it goes before its parent.
  void sift_up(std::size_t i);

  // Moves the vertex at position i down while a child goes before it,
  // always swapping with the child that goes first.
  void sift_down(std::size_t i);

public:
  explicit indexed_heap(vertex_id n, Compare comp = Compare{})
    : comp_(std::move(comp))
  {
    key_.assign(n + 1, K{});
    pos_.assign(n + 1, not_in_heap);
    heap_.reserve(n);
  }

  bool empty() const { return heap_.empty(); }
  std::size_t size() const { return heap_.size(); }
  bool contains(vertex_id v) const;
  const K& key(vertex_id v) const; // pre: contains(v)

  void push(vertex_id v, K key);           // pre: !contains(v)
  vertex_id top() const;                   // pre: !empty()
  vertex_id pop();                         // pre: !empty()
  void update_key(vertex_id v, K new_key); // pre: contains(v); either direction

  bool validate() const; // checks (I1)-(I3); meant for tests
};

// The min-heap Dijkstra uses: smallest key at the root.
template<std::floating_point K>
using indexed_min_heap = indexed_heap<K, std::less<K>>;

template<std::copyable K, typename Compare>
  requires std::strict_weak_order<Compare, const K&, const K&>
bool indexed_heap<K, Compare>::contains(vertex_id v) const
{
  assert(v >= 1 && v < pos_.size() && "contains: vertex out of range");
  return pos_[v] != not_in_heap;
}

template<std::copyable K, typename Compare>
  requires std::strict_weak_order<Compare, const K&, const K&>
const K& indexed_heap<K, Compare>::key(vertex_id v) const
{
  assert(contains(v) && "key: vertex not in heap");
  return key_[v];
}

template<std::copyable K, typename Compare>
  requires std::strict_weak_order<Compare, const K&, const K&>
void indexed_heap<K, Compare>::push(vertex_id v, K key)
{
  assert(!contains(v) && "push: vertex already in heap");

  key_[v] = std::move(key);
  heap_.push_back(v);
  pos_[v] = heap_.size() - 1;
  sift_up(pos_[v]);
}

template<std::copyable K, typename Compare>
  requires std::strict_weak_order<Compare, const K&, const K&>
vertex_id indexed_heap<K, Compare>::top() const
{
  assert(!empty() && "top: heap is empty");
  return heap_[0];
}

template<std::copyable K, typename Compare>
  requires std::strict_weak_order<Compare, const K&, const K&>
vertex_id indexed_heap<K, Compare>::pop()
{
  assert(!empty() && "pop: heap is empty");

  const vertex_id root = heap_[0];
  swap_nodes(0, heap_.size() - 1); // root to the end, last vertex to the top
  heap_.pop_back();
  pos_[root] = not_in_heap;

  if (!heap_.empty()) {
    sift_down(0);
  }
  return root;
}

template<std::copyable K, typename Compare>
  requires std::strict_weak_order<Compare, const K&, const K&>
void indexed_heap<K, Compare>::update_key(vertex_id v, K new_key)
{
  assert(contains(v) && "update_key: vertex not in heap");

  const bool moves_up = comp_(new_key, key_[v]);
  key_[v] = std::move(new_key);

  if (moves_up) {
    sift_up(pos_[v]);
  } else {
    sift_down(pos_[v]);
  }
}

template<std::copyable K, typename Compare>
  requires std::strict_weak_order<Compare, const K&, const K&>
bool indexed_heap<K, Compare>::validate() const
{
  // (I1) heap property: no child goes before its parent
  for (std::size_t i = 1; i < heap_.size(); ++i) {
    if (before(i, parent(i))) {
      return false;
    }
  }

  // (I2) every position is recorded correctly in pos_
  for (std::size_t i = 0; i < heap_.size(); ++i) {
    if (pos_[heap_[i]] != i) {
      return false;
    }
  }

  // (I3) exactly size() vertices are marked as present
  std::size_t present = 0;
  for (std::size_t v = 1; v < pos_.size(); ++v) {
    if (pos_[v] != not_in_heap) {
      ++present;
    }
  }
  return present == heap_.size();
}

template<std::copyable K, typename Compare>
  requires std::strict_weak_order<Compare, const K&, const K&>
void indexed_heap<K, Compare>::swap_nodes(std::size_t i, std::size_t j)
{
  assert(i < heap_.size() && j < heap_.size() &&
         "swap_nodes: position out of range");

  std::swap(heap_[i], heap_[j]);
  pos_[heap_[i]] = i;
  pos_[heap_[j]] = j;
}

template<std::copyable K, typename Compare>
  requires std::strict_weak_order<Compare, const K&, const K&>
void indexed_heap<K, Compare>::sift_up(std::size_t i)
{
  while (i > 0) {
    const std::size_t p = parent(i);
    if (!before(i, p)) {
      return; // i doesn't go before its parent: in place
    }
    swap_nodes(i, p);
    i = p;
  }
}

template<std::copyable K, typename Compare>
  requires std::strict_weak_order<Compare, const K&, const K&>
void indexed_heap<K, Compare>::sift_down(std::size_t i)
{
  const std::size_t n = heap_.size();
  while (true) {
    const std::size_t l = 2 * i + 1;
    const std::size_t r = 2 * i + 2;
    std::size_t first = i; // whichever of i, l, r goes first

    if (l < n && before(l, first)) {
      first = l;
    }
    if (r < n && before(r, first)) {
      first = r;
    }
    if (first == i) {
      return;
    }
    swap_nodes(i, first);
    i = first;
  }
}

} // namespace gl
