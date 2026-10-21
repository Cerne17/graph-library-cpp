#include "graph/policies.hpp"
#include "graph/types.hpp"
#include <span>

namespace gl {
template<direction_policy D, weight_policy W>
class csr_storage
{
private:
  csr_storage() = default;

  vertex_id n_ = 0;
  std::vector<edge_index> offsets_;
  std::vector<vertex_id> targets_;

public:
  using direction_type = D;
  using weighting_type = W;

  static csr_storage from_edges(vertex_id n, std::span<const edge> edges);

  vertex_id vertex_count() const;

  template<typename F>
  void for_each_neighbor(vertex_id u, F&& f) const;

  std::span<const edge_index> offsets()
    const; // non-owning read only contiguous space of memory
  std::span<const edge_index> targets() const;
};
} // namespace gl
