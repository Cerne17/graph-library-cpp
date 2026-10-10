#include "doctest/doctest.h"
#include "graph/concepts.hpp"
#include "graph/policies.hpp"
#include "graph/types.hpp"

struct mock_unweighted
{
  using direction_type = gl::undirected;
  using weighting_type = gl::unweighted;
  gl::vertex_id vertex_count() const;

  template<typename F> // callable function
  void for_each_neighbor(gl::vertex_id u, F&& func) const;
};

struct mock_broken // absent for_each_neighbor
{
  using direction_type = gl::undirected;
  using weighting_type = gl::unweighted;
  gl::vertex_id vertex_count() const;
};

struct mock_weighted
{
  using direction_type = gl::undirected;
  using weighting_type = gl::weighted<>;
  using weight_type = weighting_type::weight_type;
  gl::vertex_id vertex_count() const;

  template<typename F>
  void for_each_neighbor(gl::vertex_id u, F&& func) const;

  template<typename F>
  void for_each_edge(gl::vertex_id u, F&& func) const;
};

static_assert(gl::graph_like<mock_unweighted>);
static_assert(!gl::graph_like<mock_broken>);
static_assert(gl::graph_like<mock_unweighted> &&
              !gl::weighted_graph<mock_unweighted>);
static_assert(gl::graph_like<mock_weighted> &&
              gl::weighted_graph<mock_weighted>);
static_assert(!gl::graph_like<mock_broken> && !gl::weighted_graph<mock_broken>);

struct mock_mismatched_weight // number alias disagrees with policy
{
  using direction_type = gl::undirected;
  using weighting_type = gl::weighted<double>;
  using weight_type = float;
  gl::vertex_id vertex_count() const;

  template<typename F>
  void for_each_neighbor(gl::vertex_id u, F&& func) const;

  template<typename F>
  void for_each_edge(gl::vertex_id u, F&& func) const;
};

static_assert(gl::graph_like<mock_mismatched_weight> &&
              !gl::weighted_graph<mock_mismatched_weight>);

struct flag_off_policy // valid weight_policy: flag is false, whic is allowed
{
  static constexpr bool is_weighted = false;
  using weight_type = double;
};

struct mock_check_weight_flag
{
  using direction_type = gl::undirected;
  using weighting_type = flag_off_policy;
  gl::vertex_id vertex_count() const;

  template<typename F> // callable function
  void for_each_neighbor(gl::vertex_id u, F&& func) const;
};

static_assert(!gl::weighted_graph<mock_check_weight_flag>);
