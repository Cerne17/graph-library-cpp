// Small artificial cases: export every vertex for independent comparison.
#include "graph/algorithms/dijkstra.hpp"
#include "graph/algorithms/search.hpp"
#include "graph/graph.hpp"
#include "graph/io/edge_list.hpp"
#include "graph/policies.hpp"
#include "graph/types.hpp"

#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

template<typename G>
void export_cases(const std::string& input, gl::vertex_id source)
{
  const auto list = gl::io::read_edge_list<typename G::weighting_type>(input);
  if (source < 1 || source > list.n) throw std::runtime_error("source out of range");
  const auto graph = G::from_edges(list.n, list.edges);
  const auto bfs = gl::algorithms::bfs(graph, source);
  const auto dfs = gl::algorithms::dfs(graph, source);
  std::cout << "algorithm,vertex,value,status\n";
  for (gl::vertex_id v = 1; v <= list.n; ++v) {
    std::cout << "bfs," << v << ',' << bfs.level[v] << ",ok\n";
    std::cout << "dfs," << v << ',' << dfs.level[v] << ",ok\n";
  }
  if constexpr (G::weighting_type::is_weighted) {
    for (const auto algorithm : {"dijkstra_array", "dijkstra_heap", "dijkstra_lazy"}) {
      try {
        const auto result = std::string_view(algorithm) == "dijkstra_array" ? gl::algorithms::dijkstra_array(graph, source) :
          std::string_view(algorithm) == "dijkstra_heap" ? gl::algorithms::dijkstra_heap(graph, source) :
          gl::algorithms::dijkstra_lazy(graph, source);
        for (gl::vertex_id v = 1; v <= list.n; ++v)
          std::cout << algorithm << ',' << v << ',' << result.dist[v] << ",ok\n";
      } catch (const std::invalid_argument&) {
        std::cout << algorithm << ",0,,negative_weight\n";
      }
    }
  }
}

int main(int argc, char** argv)
{
  try {
    if (argc != 4) throw std::runtime_error("usage: baseline_cases INPUT weighted|unweighted SOURCE");
    const std::string text = argv[3];
    std::size_t used = 0;
    const auto raw = std::stoll(text, &used);
    if (used != text.size() || raw < 1 || raw > std::numeric_limits<gl::vertex_id>::max())
      throw std::runtime_error("invalid source");
    const auto source = static_cast<gl::vertex_id>(raw);
    std::cout << std::setprecision(17);
    if (std::string_view(argv[2]) == "weighted")
      export_cases<gl::csr_graph<gl::undirected, gl::weighted<double>>>(argv[1], source);
    else if (std::string_view(argv[2]) == "unweighted")
      export_cases<gl::csr_graph<gl::undirected, gl::unweighted>>(argv[1], source);
    else throw std::runtime_error("invalid weighting");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
