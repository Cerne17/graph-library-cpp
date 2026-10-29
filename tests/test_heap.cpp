#include "doctest/doctest.h"
#include "graph/heap.hpp"
#include "graph/types.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <type_traits>
#include <vector>

using heap = gl::indexed_min_heap<double>;

namespace {
template<typename K>
concept valid_heap_key = requires { typename gl::indexed_min_heap<K>; };
} // namespace

// keys must be floating-point, like edge weights
static_assert(valid_heap_key<double>);
static_assert(valid_heap_key<float>);
static_assert(!valid_heap_key<int>);

// explicit: a vertex count does not silently turn into a heap
static_assert(std::is_constructible_v<heap, gl::vertex_id>);
static_assert(!std::is_convertible_v<gl::vertex_id, heap>);

// Tests are grouped by implementation stage. Run one stage with
//   ./build/run_tests -ts="heap 2*"
// Note: every member function the tests call must have a body (even a stub),
// or the test binary fails to link.

// ---- stage 1: construction ------------------------------------------------

TEST_SUITE("heap 1: construction")
{
  TEST_CASE("heap: a new heap is empty")
  {
    const heap h(5);

    CHECK(h.empty());
    CHECK(h.size() == 0);
    CHECK(h.validate());
    for (gl::vertex_id v = 1; v <= 5; ++v) {
      CHECK_FALSE(h.contains(v));
    }
  }

  TEST_CASE("heap: a heap for zero vertices")
  {
    const heap h(0);

    CHECK(h.empty());
    CHECK(h.validate());
  }
}

// ---- stage 2: push, top, contains, key ------------------------------------

TEST_SUITE("heap 2: push and top")
{
  TEST_CASE("heap: push one vertex")
  {
    heap h(5);
    h.push(3, 2.5);

    CHECK_FALSE(h.empty());
    CHECK(h.size() == 1);
    CHECK(h.contains(3));
    CHECK_FALSE(h.contains(1));
    CHECK(h.key(3) == 2.5);
    CHECK(h.top() == 3);
    CHECK(h.validate());
  }

  TEST_CASE("heap: top is the vertex with the smallest key")
  {
    heap h(5);
    h.push(1, 4.0);
    h.push(2, 1.0);
    h.push(3, 3.0);
    h.push(4, 5.0);
    h.push(5, 2.0);

    CHECK(h.size() == 5);
    CHECK(h.top() == 2);
    CHECK(h.validate());
  }

  TEST_CASE("heap: pushing decreasing keys moves each one to the root")
  {
    heap h(6);
    for (gl::vertex_id v = 1; v <= 6; ++v) {
      h.push(v, 10.0 - v); // every push is a new minimum
      CHECK(h.top() == v);
      CHECK(h.validate());
    }
  }

  TEST_CASE("heap: pushing increasing keys keeps the first at the root")
  {
    heap h(6);
    for (gl::vertex_id v = 1; v <= 6; ++v) {
      h.push(v, static_cast<double>(v));
      CHECK(h.top() == 1);
      CHECK(h.validate());
    }
  }

  TEST_CASE("heap: top does not remove")
  {
    heap h(3);
    h.push(1, 1.0);
    h.push(2, 2.0);

    CHECK(h.top() == 1);
    CHECK(h.top() == 1);
    CHECK(h.size() == 2);
    CHECK(h.contains(1));
  }

  TEST_CASE("heap: key returns each vertex's own key")
  {
    heap h(4);
    h.push(4, 0.25);
    h.push(1, 7.0);
    h.push(2, -3.0);

    CHECK(h.key(4) == 0.25);
    CHECK(h.key(1) == 7.0);
    CHECK(h.key(2) == -3.0);
  }

  TEST_CASE("heap: negative, zero and infinite keys")
  {
    constexpr double inf = std::numeric_limits<double>::infinity();

    heap h(4);
    h.push(1, inf);
    h.push(2, 0.0);
    h.push(3, -1.5);
    h.push(4, inf);

    CHECK(h.top() == 3);
    CHECK(h.key(1) == inf);
    CHECK(h.validate());
  }
}

// ---- stage 3: pop ---------------------------------------------------------

TEST_SUITE("heap 3: pop")
{
  TEST_CASE("heap: pop the only vertex")
  {
    heap h(3);
    h.push(2, 1.0);

    CHECK(h.pop() == 2);
    CHECK(h.empty());
    CHECK_FALSE(h.contains(2));
    CHECK(h.validate());
  }

  TEST_CASE("heap: pop returns vertices in ascending key order")
  {
    heap h(5);
    h.push(1, 4.0);
    h.push(2, 1.0);
    h.push(3, 3.0);
    h.push(4, 5.0);
    h.push(5, 2.0);

    const std::vector<gl::vertex_id> expected{ 2, 5, 3, 1, 4 };
    for (std::size_t i = 0; i < expected.size(); ++i) {
      CHECK(h.size() == expected.size() - i);
      CHECK(h.pop() == expected[i]);
      CHECK(h.validate());
    }
    CHECK(h.empty());
  }

  TEST_CASE("heap: a popped vertex is no longer contained")
  {
    heap h(3);
    h.push(1, 1.0);
    h.push(2, 2.0);
    h.push(3, 3.0);

    h.pop();

    CHECK_FALSE(h.contains(1));
    CHECK(h.contains(2));
    CHECK(h.contains(3));
  }

  TEST_CASE("heap: equal keys are all popped, in non-decreasing order")
  {
    heap h(6);
    h.push(1, 2.0);
    h.push(2, 1.0);
    h.push(3, 2.0);
    h.push(4, 1.0);
    h.push(5, 2.0);
    h.push(6, 1.0);

    std::vector<bool> seen(7, false);
    double last = -std::numeric_limits<double>::infinity();
    while (!h.empty()) {
      const double k = h.key(h.top());
      const gl::vertex_id v = h.pop();
      CHECK(k >= last);
      CHECK_FALSE(seen[v]);
      seen[v] = true;
      last = k;
      CHECK(h.validate());
    }
    for (gl::vertex_id v = 1; v <= 6; ++v) {
      CHECK(seen[v]);
    }
  }

  TEST_CASE("heap: pushes and pops interleaved")
  {
    heap h(5);
    h.push(1, 5.0);
    h.push(2, 3.0);
    CHECK(h.pop() == 2);

    h.push(3, 4.0);
    h.push(4, 1.0);
    CHECK(h.pop() == 4);
    CHECK(h.pop() == 3);

    h.push(5, 6.0);
    CHECK(h.pop() == 1);
    CHECK(h.pop() == 5);
    CHECK(h.empty());
  }

  TEST_CASE("heap: a popped vertex can be pushed again")
  {
    heap h(2);
    h.push(1, 1.0);
    h.push(2, 2.0);
    CHECK(h.pop() == 1);

    h.push(1, 3.0);

    CHECK(h.contains(1));
    CHECK(h.key(1) == 3.0);
    CHECK(h.top() == 2);
    CHECK(h.validate());
  }

  TEST_CASE("heap: many vertices come out sorted (heap sort)")
  {
    constexpr gl::vertex_id n = 500;
    std::mt19937 rng(42);

    heap h(n);
    for (gl::vertex_id v = 1; v <= n; ++v) {
      h.push(v, static_cast<double>(rng() % 1000));
    }
    REQUIRE(h.validate());

    double last = -1.0;
    std::size_t popped = 0;
    while (!h.empty()) {
      const double k = h.key(h.top());
      h.pop();
      CHECK(k >= last);
      last = k;
      ++popped;
    }
    CHECK(popped == n);
    CHECK(h.validate());
  }
}

// ---- stage 4: decrease_key ------------------------------------------------

TEST_SUITE("heap 4: decrease_key")
{
  TEST_CASE("heap: decrease_key updates the key")
  {
    heap h(3);
    h.push(1, 5.0);
    h.push(2, 6.0);

    h.update_key(2, 5.5);

    CHECK(h.key(2) == 5.5);
    CHECK(h.top() == 1);
    CHECK(h.validate());
  }

  TEST_CASE("heap: decrease_key below the minimum makes it the top")
  {
    heap h(5);
    for (gl::vertex_id v = 1; v <= 5; ++v) {
      h.push(v, 10.0 * v);
    }

    h.update_key(5, 1.0); // a leaf moves all the way up

    CHECK(h.top() == 5);
    CHECK(h.size() == 5);
    CHECK(h.validate());
  }

  TEST_CASE("heap: decrease_key on the root")
  {
    heap h(3);
    h.push(1, 2.0);
    h.push(2, 3.0);

    h.update_key(1, 0.5);

    CHECK(h.top() == 1);
    CHECK(h.key(1) == 0.5);
    CHECK(h.validate());
  }

  TEST_CASE("heap: decrease_key from infinity, as Dijkstra does")
  {
    constexpr double inf = std::numeric_limits<double>::infinity();

    heap h(4);
    for (gl::vertex_id v = 1; v <= 4; ++v) {
      h.push(v, inf);
    }
    h.update_key(3, 2.0);
    h.update_key(4, 1.0);

    CHECK(h.pop() == 4);
    CHECK(h.pop() == 3);
    CHECK(h.key(h.top()) == inf);
    CHECK(h.validate());
  }

  TEST_CASE("heap: pop order follows the decreased keys")
  {
    heap h(5);
    h.push(1, 1.0);
    h.push(2, 2.0);
    h.push(3, 3.0);
    h.push(4, 4.0);
    h.push(5, 5.0);

    h.update_key(4, 0.5);
    h.update_key(5, 1.5);

    const std::vector<gl::vertex_id> expected{ 4, 1, 5, 2, 3 };
    for (const gl::vertex_id v : expected) {
      CHECK(h.pop() == v);
      CHECK(h.validate());
    }
  }

  TEST_CASE("heap: float keys")
  {
    gl::indexed_min_heap<float> h(3);
    h.push(1, 3.0f);
    h.push(2, 2.0f);
    h.push(3, 1.0f);

    h.update_key(1, 0.5f);

    CHECK(h.key(1) == 0.5f);
    CHECK(h.pop() == 1);
    CHECK(h.pop() == 3);
    CHECK(h.validate());
  }
}

// ---- stage 5: everything against a reference ------------------------------

TEST_SUITE("heap 5: randomized")
{
  TEST_CASE("heap: random operations match a brute-force reference")
  {
    constexpr gl::vertex_id n = 200;
    constexpr int steps = 5000;
    std::mt19937 rng(7);

    heap h(n);
    std::vector<double> ref_key(n + 1);
    std::vector<bool> ref_in(n + 1, false);
    std::size_t ref_size = 0;

    // smallest key among the vertices the reference holds
    auto ref_min_key = [&] {
      double best = std::numeric_limits<double>::infinity();
      for (gl::vertex_id v = 1; v <= n; ++v) {
        if (ref_in[v] && ref_key[v] < best) {
          best = ref_key[v];
        }
      }
      return best;
    };

    for (int step = 0; step < steps; ++step) {
      const gl::vertex_id v = 1 + rng() % n;
      const auto op = rng() % 3;

      if (op == 0 && !ref_in[v]) {
        const double k = static_cast<double>(rng() % 10000);
        h.push(v, k);
        ref_key[v] = k;
        ref_in[v] = true;
        ++ref_size;
      } else if (op == 1 && ref_size > 0) {
        const double expected = ref_min_key();
        REQUIRE(h.key(h.top()) == expected);
        const gl::vertex_id u = h.pop();
        REQUIRE(ref_in[u]);
        REQUIRE(ref_key[u] == expected);
        ref_in[u] = false;
        --ref_size;
      } else if (op == 2 && ref_in[v] && ref_key[v] > 0.0) {
        const double k = static_cast<double>(rng() % static_cast<std::uint32_t>(ref_key[v]));
        h.update_key(v, k);
        ref_key[v] = k;
      }

      REQUIRE(h.size() == ref_size);
      REQUIRE(h.empty() == (ref_size == 0));
      REQUIRE(h.contains(v) == ref_in[v]);
      if (ref_in[v]) {
        REQUIRE(h.key(v) == ref_key[v]);
      }
      REQUIRE(h.validate());
    }
  }
}
