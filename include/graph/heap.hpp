#pragma once

#include "graph/types.hpp"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace gl {

// Binary min-heap over vertices 1..n, with keys stored per vertex and a
// position map so a vertex's key can be lowered in O(log n).
//
// Invariants (checked by validate()):
//   (I1) heap property: key of parent(i) <= key of i, for every position i > 0
//   (I2) positions match: pos_[heap_[i]] == i, for every position i
//   (I3) absent vertices: pos_[v] == not_in_heap for every v not in heap_
template<std::floating_point K>
class indexed_min_heap
{
private:
  static constexpr std::size_t not_in_heap =
    std::numeric_limits<std::size_t>::max();

  std::vector<vertex_id> heap_;  // heap_[i]: vertex at position i (0-based)
  std::vector<K> key_;           // key_[v]: current key of vertex v (1-based)
  std::vector<std::size_t> pos_; // pos_[v]: position of v, or not_in_heap

  static std::size_t parent(std::size_t i) { return (i - 1) / 2; }

  // key of the vertex sitting at position i
  K key_at(std::size_t i) const { return key_[heap_[i]]; }

  // Swaps the vertices at positions i and j, and keeps pos_ in sync.
  // Every move inside the heap goes through here.
  void swap_nodes(std::size_t i, std::size_t j);

  // Moves the vertex at position i up while its key is smaller than its
  // parent's.
  void sift_up(std::size_t i);

  // Moves the vertex at position i down while a child has a smaller key,
  // always swapping with the smaller child.
  void sift_down(std::size_t i);

public:
  explicit indexed_min_heap(vertex_id n)
  {
    key_.assign(n + 1, K{});
    pos_.assign(n + 1, not_in_heap);
    heap_.reserve(n);
  }

  bool empty() const { return heap_.empty(); }
  std::size_t size() const { return heap_.size(); }
  bool contains(vertex_id v) const;
  K key(vertex_id v) const; // pre: contains(v)

  void push(vertex_id v, K key);             // pre: !contains(v)
  vertex_id top() const;                     // pre: !empty()
  vertex_id pop();                           // pre: !empty()
  void decrease_key(vertex_id v, K new_key); // pre: contains(v), new_key <= key(v)

  bool validate() const; // checks (I1)-(I3); meant for tests
};

template<std::floating_point K>
bool indexed_min_heap<K>::contains(vertex_id v) const
{
  assert(v >= 1 && v < pos_.size() && "contains: vertex out of range");
  return pos_[v] != not_in_heap;
}

template<std::floating_point K>
K indexed_min_heap<K>::key(vertex_id v) const
{
  assert(contains(v) && "key: vertex not in heap");
  return key_[v];
}

template<std::floating_point K>
void indexed_min_heap<K>::push(vertex_id v, K key)
{
  assert(!contains(v) && "push: vertex already in heap");

  key_[v] = key;
  heap_.push_back(v);
  pos_[v] = heap_.size() - 1;
  sift_up(pos_[v]);
}

template<std::floating_point K>
vertex_id indexed_min_heap<K>::top() const
{
  assert(!empty() && "top: heap is empty");
  return heap_[0];
}

template<std::floating_point K>
vertex_id indexed_min_heap<K>::pop()
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

template<std::floating_point K>
void indexed_min_heap<K>::decrease_key(vertex_id v, K new_key)
{
  assert(contains(v) && "decrease_key: vertex not in heap");
  assert(new_key <= key_[v] && "decrease_key: new key is larger");

  key_[v] = new_key;
  sift_up(pos_[v]);
}

template<std::floating_point K>
bool indexed_min_heap<K>::validate() const
{
  // (I1) heap property, for every position that has a parent
  for (std::size_t i = 1; i < heap_.size(); ++i) {
    if (key_at(parent(i)) > key_at(i)) {
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

template<std::floating_point K>
void indexed_min_heap<K>::swap_nodes(std::size_t i, std::size_t j)
{
  assert(i < heap_.size() && j < heap_.size() &&
         "swap_nodes: position out of range");

  std::swap(heap_[i], heap_[j]);
  pos_[heap_[i]] = i;
  pos_[heap_[j]] = j;
}

template<std::floating_point K>
void indexed_min_heap<K>::sift_up(std::size_t i)
{
  while (i > 0) {
    const std::size_t p = parent(i);
    if (key_at(p) <= key_at(i)) {
      return;
    }
    swap_nodes(i, p);
    i = p;
  }
}

template<std::floating_point K>
void indexed_min_heap<K>::sift_down(std::size_t i)
{
  const std::size_t n = heap_.size();
  while (true) {
    const std::size_t l = 2 * i + 1;
    const std::size_t r = 2 * i + 2;
    std::size_t smallest = i;

    if (l < n && key_at(l) < key_at(smallest)) {
      smallest = l;
    }
    if (r < n && key_at(r) < key_at(smallest)) {
      smallest = r;
    }
    if (smallest == i) {
      return;
    }
    swap_nodes(i, smallest);
    i = smallest;
  }
}

} // namespace gl
