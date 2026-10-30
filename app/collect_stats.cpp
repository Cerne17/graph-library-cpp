// Dataset exports and measurements for both course parts, on the new library.
#include "graph/algorithms/dijkstra.hpp"
#include "graph/algorithms/distance.hpp"
#include "graph/algorithms/path.hpp"
#include "graph/algorithms/search.hpp"
#include "graph/concepts.hpp"
#include "graph/graph.hpp"
#include "graph/io/edge_list.hpp"
#include "graph/io/label_map.hpp"
#include "graph/policies.hpp"
#include "graph/types.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#ifdef __GLIBC__
#include <malloc.h>
#endif
#ifdef __APPLE__
#include <mach/mach.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace {
struct options {
  std::string input, weighting, representation, phase;
  std::filesystem::path output;
  std::string algorithm = "bfs", labels, source;
  std::uint32_t seed = 2025;
  std::size_t k = 100, sample_nodes = 1000, sample_edges = 4000;
  std::uint64_t exact_work = 100000000;
};

long long integer(const std::string& text) {
  std::size_t used = 0;
  const auto value = std::stoll(text, &used);
  if (used != text.size() || value < 0) throw std::invalid_argument("expected a nonnegative integer: " + text);
  return value;
}
options parse(int argc, char** argv) {
  if (argc < 6) throw std::invalid_argument("usage: collect_stats INPUT weighted|unweighted csr|matrix stats|timing|cases|mem OUT [--key value]...");
  options o;
  o.input = argv[1]; o.weighting = argv[2]; o.representation = argv[3];
  o.phase = argv[4]; o.output = argv[5];
  for (int i = 6; i < argc; i += 2) {
    if (i + 1 == argc) throw std::invalid_argument("missing option value");
    const std::string flag = argv[i], value = argv[i + 1];
    if (flag == "--algorithm") o.algorithm = value;
    else if (flag == "--labels") o.labels = value;
    else if (flag == "--source") o.source = value;
    else if (flag == "--seed") {
      const auto n = integer(value);
      if (n > std::numeric_limits<std::uint32_t>::max()) throw std::invalid_argument("seed out of range");
      o.seed = static_cast<std::uint32_t>(n);
    } else if (flag == "--k") o.k = integer(value);
    else if (flag == "--sample-nodes") o.sample_nodes = integer(value);
    else if (flag == "--sample-edges") o.sample_edges = integer(value);
    else if (flag == "--exact-work") o.exact_work = integer(value);
    else throw std::invalid_argument("unknown option: " + flag);
  }
  if (o.weighting != "weighted" && o.weighting != "unweighted") throw std::invalid_argument("invalid weighting");
  if (o.representation != "csr" && o.representation != "matrix") throw std::invalid_argument("invalid representation");
  if (o.phase != "stats" && o.phase != "timing" && o.phase != "cases" && o.phase != "mem") throw std::invalid_argument("invalid phase");
  if (o.k == 0) throw std::invalid_argument("k must be positive");
  if (o.phase == "stats" && o.representation != "csr") throw std::invalid_argument("structural exports use CSR");
  return o;
}

std::string csv(std::string_view s) {
  std::string out = "\"";
  for (const char c : s) { if (c == '"') out += '"'; out += c; }
  return out + '"';
}
std::ofstream file(const options& o, const std::string& name) {
  std::ofstream out(o.output / name);
  out.exceptions(std::ios::badbit | std::ios::failbit);
  out << std::setprecision(17);
  return out;
}
void ready() {
  std::uint64_t rss = 0;
#ifdef __APPLE__
  mach_task_basic_info_data_t info{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
    throw std::runtime_error("could not measure process RSS");
  rss = info.resident_size;
#elif defined(__linux__)
  std::ifstream status("/proc/self/statm");
  std::uint64_t pages = 0, resident = 0;
  const auto page_size = sysconf(_SC_PAGESIZE);
  if (!(status >> pages >> resident) || page_size <= 0)
    throw std::runtime_error("could not measure process RSS");
  rss = resident * static_cast<std::uint64_t>(page_size);
#else
  throw std::runtime_error("RSS measurement requires macOS or Linux");
#endif
  std::cout << "READY," << rss << '\n' << std::flush;
  std::string line;
  if (!std::getline(std::cin, line)) throw std::runtime_error("measurement controller disconnected");
}

// Component labels avoid retaining another copy of every vertex in a nested
// component vector. IDs follow discovery order, not component size order.
struct topology {
  std::vector<gl::vertex_id> component;
  std::vector<gl::vertex_id> roots;
  std::vector<std::uint64_t> sizes;
};
template<gl::graph_like G>
topology components(const G& g) {
  topology t{std::vector<gl::vertex_id>(g.vertex_count() + 1, 0), {0}, {0}};
  std::queue<gl::vertex_id> queue;
  for (gl::vertex_id s = 1; s <= g.vertex_count(); ++s) {
    if (t.component[s]) continue;
    const auto id = static_cast<gl::vertex_id>(t.sizes.size());
    t.roots.push_back(s); t.sizes.push_back(0);
    t.component[s] = id; queue.push(s);
    while (!queue.empty()) {
      const auto u = queue.front(); queue.pop(); ++t.sizes[id];
      g.for_each_neighbor(u, [&](auto v) {
        if (!t.component[v]) { t.component[v] = id; queue.push(v); }
      });
    }
  }
  return t;
}

gl::vertex_id resolve_source(const options& o, gl::vertex_id n,
                              const gl::io::label_map* labels) {
  if (o.source.empty()) return std::min<gl::vertex_id>(10, n);
  if (labels) return labels->id_of(o.source);
  const auto source = integer(o.source);
  if (source < 1 || source > n) throw std::invalid_argument("source out of range");
  return static_cast<gl::vertex_id>(source);
}

struct sample_edge { gl::vertex_id u, v; double weight; };
template<gl::graph_like G>
void network_sample(const G& g, const options& o, const topology& t,
                    const std::vector<gl::edge_index>& degrees,
                    const gl::io::label_map* labels, const std::string& name,
                    const std::vector<gl::vertex_id>& seeds) {
  const auto limit = std::min<std::size_t>(o.sample_nodes, g.vertex_count());
  std::set<gl::vertex_id> selected;
  std::queue<gl::vertex_id> frontier;
  for (auto s : seeds) {
    if (selected.size() >= limit) break;
    if (selected.insert(s).second) frontier.push(s);
  }
  while (!frontier.empty() && selected.size() < limit) {
    auto u = frontier.front(); frontier.pop();
    g.for_each_neighbor(u, [&](auto v) {
      if (selected.size() < limit && selected.insert(v).second) frontier.push(v);
    });
  }
  auto nodes = file(o, name + "_nodes.csv");
  nodes << "id,label,degree,component,component_size\n";
  for (auto u : selected) nodes << u << ',' << csv(labels ? labels->name_of(u) : "") << ','
    << degrees[u] << ',' << t.component[u] << ',' << t.sizes[t.component[u]] << '\n';
  // Reservoir sampling of the induced edges keeps output bounded without
  // privileging the lowest vertex IDs when the edge cap is reached.
  std::mt19937 rng(o.seed);
  std::uint64_t seen = 0;
  std::vector<sample_edge> edges;
  auto accept = [&](gl::vertex_id u, gl::vertex_id v, double w) {
    if (v <= u || !selected.contains(v)) return;
    ++seen;
    if (edges.size() < o.sample_edges) edges.push_back({u, v, w});
    else if (o.sample_edges) {
      const auto slot = std::uniform_int_distribution<std::uint64_t>(0, seen - 1)(rng);
      if (slot < edges.size()) edges[slot] = {u, v, w};
    }
  };
  for (auto u : selected) {
    if constexpr (G::weighting_type::is_weighted)
      g.for_each_edge(u, [&](auto v, auto w) { accept(u, v, w); });
    else g.for_each_neighbor(u, [&](auto v) { accept(u, v, 1); });
  }
  auto out = file(o, name + "_edges.csv"); out << "source,target,weight\n";
  for (auto e : edges) out << e.u << ',' << e.v << ',' << e.weight << '\n';
  auto meta = file(o, name + "_sample.csv");
  meta << "method,seed,nodes,node_limit,induced_edges,exported_edges,edge_limit,seeds\n"
       << csv(name == "neighborhood" ? "bounded_bfs" : "component_seeded_bfs") << ','
       << o.seed << ',' << selected.size() << ',' << limit << ',' << seen << ','
       << edges.size() << ',' << o.sample_edges << ',';
  std::ostringstream seed_text;
  for (auto s : seeds) seed_text << s << ' ';
  meta << csv(seed_text.str()) << '\n';
}

template<gl::graph_like G>
void export_stats(const G& g, const options& o, std::size_t input_edges,
                  std::size_t loops, bool negative_loop,
                  const gl::io::label_map* labels) {
  const auto n = g.vertex_count();
  const auto t = components(g);
  std::vector<gl::edge_index> degrees(n + 1, 0);
  std::map<gl::edge_index, std::uint64_t> histogram;
  std::map<std::uint64_t, std::uint64_t> component_hist;
  gl::edge_index arcs = 0, duplicate_arcs = 0;
  double min_w = std::numeric_limits<double>::infinity(), max_w = -min_w;
  long double mean_w = 0;
  std::uint64_t weight_count = 0, zero_weights = 0, negative_weights = 0;
  for (gl::vertex_id u = 1; u <= n; ++u) {
    gl::vertex_id last = 0;
    auto visit = [&](gl::vertex_id v, double w) {
      ++degrees[u]; ++arcs; duplicate_arcs += v == last; last = v;
      if (u < v) {
        min_w = std::min(min_w, w); max_w = std::max(max_w, w);
        ++weight_count;
        mean_w = std::lerp(mean_w, static_cast<long double>(w), 1.L / weight_count);
        zero_weights += w == 0; negative_weights += w < 0;
      }
    };
    if constexpr (G::weighting_type::is_weighted) g.for_each_edge(u, visit);
    else g.for_each_neighbor(u, [&](auto v) { visit(v, 1); });
    ++histogram[degrees[u]];
  }
  auto vertex_file = file(o, "vertices.csv"); vertex_file << "id,degree,component\n";
  for (gl::vertex_id u = 1; u <= n; ++u)
    vertex_file << u << ',' << degrees[u] << ',' << t.component[u] << '\n';
  vertex_file.close();
  auto degree_file = file(o, "degree_histogram.csv"); degree_file << "degree,vertices\n";
  for (auto [degree, count] : histogram) degree_file << degree << ',' << count << '\n';
  auto component_file = file(o, "components.csv"); component_file << "component,vertices,root\n";
  std::vector<gl::vertex_id> ranked;
  for (gl::vertex_id id = 1; id < t.sizes.size(); ++id) ranked.push_back(id);
  std::stable_sort(ranked.begin(), ranked.end(), [&](auto a, auto b) { return t.sizes[a] > t.sizes[b]; });
  for (auto id : ranked) { component_file << id << ',' << t.sizes[id] << ',' << t.roots[id] << '\n'; ++component_hist[t.sizes[id]]; }
  auto comp_hist_file = file(o, "component_histogram.csv"); comp_hist_file << "vertices,components\n";
  for (auto [size, count] : component_hist) comp_hist_file << size << ',' << count << '\n';
  auto quantile = [&](std::uint64_t rank) {
    std::uint64_t count = 0;
    for (auto [degree, frequency] : histogram) { count += frequency; if (count > rank) return degree; }
    return gl::edge_index{0};
  };
  auto summary = file(o, "summary.csv"); summary << "metric,value\n";
  auto metric = [&](std::string_view key, auto value) { summary << key << ',' << value << '\n'; };
  metric("vertices", n); metric("input_edges", input_edges + loops);
  metric("skipped_self_loops", loops); metric("negative_self_loop", negative_loop);
  metric("edges", arcs / 2); metric("parallel_edges", duplicate_arcs / 2);
  metric("unique_edges", (arcs - duplicate_arcs) / 2);
  metric("density_simple", n > 1 ? static_cast<double>(arcs - duplicate_arcs) / n / (n - 1) : 0);
  metric("degree_min", histogram.empty() ? 0 : histogram.begin()->first);
  metric("degree_max", histogram.empty() ? 0 : histogram.rbegin()->first);
  metric("degree_mean", n ? static_cast<double>(arcs) / n : 0);
  metric("degree_median", n ? (static_cast<double>(quantile((n - 1) / 2)) + quantile(n / 2)) / 2 : 0);
  metric("isolated_vertices", histogram.contains(0) ? histogram.at(0) : 0);
  metric("component_count", ranked.size());
  metric("component_largest", ranked.empty() ? 0 : t.sizes[ranked.front()]);
  metric("component_smallest", ranked.empty() ? 0 : t.sizes[ranked.back()]);
  if constexpr (G::weighting_type::is_weighted) {
    metric("negative_edges", negative_weights); metric("zero_weight_edges", zero_weights);
    if (weight_count) {
      metric("weight_min", min_w); metric("weight_max", max_w);
      metric("weight_mean", mean_w);
    }
    constexpr std::size_t bins = 50;
    std::vector<std::uint64_t> counts(bins, 0);
    if (weight_count) for (gl::vertex_id u = 1; u <= n; ++u)
      g.for_each_edge(u, [&](auto v, double w) {
        if (u >= v) return;
        // On platforms where long double is double, max-min can overflow
        // even though both endpoints are finite. Scale before subtraction.
        const long double range = static_cast<long double>(max_w) - min_w;
        const long double fraction = max_w == min_w ? 0 :
          std::isfinite(range) ? (static_cast<long double>(w) - min_w) / range :
          (static_cast<long double>(w) / 2 - static_cast<long double>(min_w) / 2) /
          (static_cast<long double>(max_w) / 2 - static_cast<long double>(min_w) / 2);
        const auto bin = std::min<std::size_t>(bins - 1, static_cast<std::size_t>(fraction * bins));
        ++counts[bin];
      });
    auto weights = file(o, "weight_histogram.csv"); weights << "lower,upper,edges\n";
    if (weight_count) for (std::size_t i = 0; i < bins; ++i) {
      const auto lo = std::lerp(min_w, max_w, static_cast<double>(i) / bins);
      const auto hi = std::lerp(min_w, max_w, static_cast<double>(i + 1) / bins);
      weights << lo << ',' << hi << ',' << counts[i] << '\n';
    }
  }
  // Exact diameter is global and uses hop distances, including weighted input.
  const long double work = static_cast<long double>(n) * (n + arcs);
  auto diameter = file(o, "diameter.csv"); diameter << "method,scope,distance_kind,value,status,work_estimate\n";
  if (n && o.exact_work && work <= o.exact_work)
    diameter << "exact,all_components,hops," << gl::algorithms::exact_diameter(g) << ",ok," << work << '\n';
  else diameter << "exact,all_components,hops,," << (n ? "budget_exceeded" : "empty") << ',' << work << '\n';
  if (n) {
    const auto first = gl::algorithms::bfs(g, t.roots[ranked.front()]);
    auto farthest = first.source;
    for (gl::vertex_id v = 1; v <= n; ++v) if (first.level[v] > first.level[farthest]) farthest = v;
    const auto second = gl::algorithms::bfs(g, farthest);
    diameter << "double_sweep,largest_component,hops," << *std::max_element(second.level.begin(), second.level.end()) << ",lower_bound,\n";
  }
  if (n && o.sample_nodes) {
    const auto source = resolve_source(o, n, labels);
    network_sample(g, o, t, degrees, labels, "neighborhood", {source});
    std::vector<gl::vertex_id> seeds;
    const auto count = std::min<std::size_t>({16, ranked.size(), o.sample_nodes});
    for (std::size_t i = 0; i < count; ++i) {
      const auto rank = count == 1 ? 0 : i * (ranked.size() - 1) / (count - 1);
      seeds.push_back(t.roots[ranked[rank]]);
    }
    network_sample(g, o, t, degrees, labels, "overview", seeds);
  }
}

std::vector<gl::vertex_id> sources(gl::vertex_id n, const options& o) {
  std::vector<gl::vertex_id> result;
  if (o.k >= n) { for (gl::vertex_id v = 1; v <= n; ++v) result.push_back(v); return result; }
  std::mt19937 rng(o.seed); std::set<gl::vertex_id> chosen;
  while (result.size() < o.k) {
    const auto v = std::uniform_int_distribution<gl::vertex_id>(1, n)(rng);
    if (chosen.insert(v).second) result.push_back(v);
  }
  return result;
}
template<gl::graph_like G>
void timing(const G& g, const options& o, bool negative_loop) {
  const bool search = o.algorithm == "bfs" || o.algorithm == "dfs";
  const bool dijkstra = o.algorithm == "dijkstra_array" || o.algorithm == "dijkstra_heap" || o.algorithm == "dijkstra_lazy";
  if (!search && !dijkstra) throw std::invalid_argument("unknown algorithm");
  if constexpr (!G::weighting_type::is_weighted) {
    if (dijkstra) throw std::invalid_argument("Dijkstra requires weighted input");
  }
  auto out = file(o, "timings.csv");
  out << "algorithm,source,seed,elapsed_micros,reachable_vertices,status\n";
  for (const auto source : sources(g.vertex_count(), o)) {
    const auto start = std::chrono::steady_clock::now();
    double elapsed = 0; std::size_t reachable = 0;
    if (search) {
      const auto tree = o.algorithm == "bfs" ? gl::algorithms::bfs(g, source) : gl::algorithms::dfs(g, source);
      const auto end = std::chrono::steady_clock::now();
      elapsed = std::chrono::duration<double, std::micro>(end - start).count();
      reachable = std::count_if(tree.level.begin() + 1, tree.level.end(), [](auto d) { return d >= 0; });
    } else if constexpr (G::weighting_type::is_weighted) {
      if (negative_loop || g.has_negative_weight()) {
        out << o.algorithm << ',' << source << ',' << o.seed << ",,,not_implemented\n"; continue;
      }
      const auto tree = o.algorithm == "dijkstra_array" ? gl::algorithms::dijkstra_array(g, source) :
                        o.algorithm == "dijkstra_heap" ? gl::algorithms::dijkstra_heap(g, source) : gl::algorithms::dijkstra_lazy(g, source);
      const auto end = std::chrono::steady_clock::now();
      elapsed = std::chrono::duration<double, std::micro>(end - start).count();
      reachable = std::count_if(tree.dist.begin() + 1, tree.dist.end(), [](auto d) { return std::isfinite(d); });
    }
    out << o.algorithm << ',' << source << ',' << o.seed << ',' << elapsed << ',' << reachable << ",ok\n" << std::flush;
  }
}

template<gl::graph_like G>
void cases(const G& g, const options& o, bool negative_loop, const gl::io::label_map* labels) {
  auto out = file(o, "cases.csv");
  out << "algorithm,source,target,source_name,target_name,parent,distance,hops,path,status\n";
  auto row = [&](std::string_view alg, auto source, auto target, auto parent, auto distance,
                 auto hops, const auto& path, const std::string& status) {
    out << alg << ',' << source << ',' << target << ',' << csv(labels ? labels->name_of(source) : "")
        << ',' << csv(labels ? labels->name_of(target) : "") << ',' << parent << ',' << distance << ',' << hops << ',';
    std::ostringstream text; for (auto v : path) text << v << ' ';
    out << csv(text.str()) << ',' << status << '\n';
  };
  if constexpr (G::weighting_type::is_weighted) {
    if (!g.vertex_count()) return;
    const auto source = resolve_source(o, g.vertex_count(), labels);
    const bool negative = negative_loop || g.has_negative_weight();
    std::optional<gl::algorithms::shortest_paths<double>> tree;
    if (!negative) tree = gl::algorithms::dijkstra_heap(g, source);
    std::vector<gl::vertex_id> targets;
    if (labels) {
      for (const auto name : {"Alan M. Turing", "J. B. Kruskal", "Jon M. Kleinberg", "Éva Tardos", "Daniel R. Figueiredo"}) {
        try { targets.push_back(labels->id_of(name)); }
        catch (const std::invalid_argument&) { throw std::runtime_error(std::string("missing case-study name: ") + name); }
      }
    } else for (auto v : {20u, 30u, 40u, 50u, 60u}) if (v <= g.vertex_count()) targets.push_back(v);
    for (auto target : targets) {
      if (negative) row("dijkstra_heap", source, target, 0, "", "", std::vector<gl::vertex_id>{}, "not_implemented");
      else {
        const auto path = gl::algorithms::reconstruct_path(*tree, target);
        row("dijkstra_heap", source, target, tree->parent[target], tree->dist[target],
            path.empty() ? std::string() : std::to_string(path.size() - 1), path, path.empty() ? "unreachable" : "ok");
      }
    }
  } else {
    for (auto s : {1u, 2u, 3u}) if (s <= g.vertex_count()) {
      const auto bfs = gl::algorithms::bfs(g, s), dfs = gl::algorithms::dfs(g, s);
      for (auto v : {10u, 20u, 30u}) if (v <= g.vertex_count()) {
        row("bfs", s, v, bfs.parent[v], bfs.level[v], bfs.level[v], gl::algorithms::reconstruct_path(bfs, v), bfs.level[v] < 0 ? "unreachable" : "ok");
        row("dfs", s, v, dfs.parent[v], "", dfs.level[v], gl::algorithms::reconstruct_path(dfs, v), dfs.level[v] < 0 ? "unreachable" : "ok");
      }
    }
    for (auto [s, v] : {std::pair{10u, 20u}, {10u, 30u}, {20u, 30u}}) if (v <= g.vertex_count()) {
      const auto tree = gl::algorithms::bfs(g, s);
      row("bfs_distance", s, v, tree.parent[v], tree.level[v], tree.level[v], gl::algorithms::reconstruct_path(tree, v), tree.level[v] < 0 ? "unreachable" : "ok");
    }
  }
}

template<typename G>
int run(const options& o) {
  std::optional<G> graph;
  std::size_t edge_count = 0, loops = 0;
  bool negative_loop = false;
  {
    const auto list = gl::io::read_edge_list<typename G::weighting_type>(o.input);
    edge_count = list.edges.size(); loops = list.skipped_self_loops;
    if constexpr (G::weighting_type::is_weighted) {
      for (const auto& e : list.edges) if (!std::isfinite(e.weight)) throw std::runtime_error("non-finite edge weight");
      // The shared reader intentionally drops loops. Preserve the fact that
      // a negative one existed so this driver never runs Dijkstra on it.
      if (loops) {
        std::ifstream input(o.input); input.exceptions(std::ios::badbit);
        std::string line;
        while (std::getline(input, line)) {
          std::istringstream fields(line); long long u, v; double w;
          if (fields >> u >> v >> w && u == v && w < 0) negative_loop = true;
        }
      }
    }
    graph.emplace(G::from_edges(list.n, list.edges));
  }
#ifdef __GLIBC__
  malloc_trim(0);
#endif
  ready();
  if (o.phase == "mem") return 0;
  std::optional<gl::io::label_map> labels;
  if (!o.labels.empty()) {
    labels = gl::io::read_label_map(o.labels);
    if (labels->size() != graph->vertex_count()) throw std::runtime_error("label count differs from vertex count");
  }
  std::filesystem::create_directories(o.output);
  if (o.phase == "stats") {
    if constexpr (requires { graph->offsets(); }) export_stats(*graph, o, edge_count, loops, negative_loop, labels ? &*labels : nullptr);
  } else if (o.phase == "timing") timing(*graph, o, negative_loop);
  else cases(*graph, o, negative_loop, labels ? &*labels : nullptr);
  return 0;
}
} // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string_view(argv[1]) == "--physical-memory") {
#ifdef __APPLE__
      host_basic_info_data_t info{};
      mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
      if (host_info(mach_host_self(), HOST_BASIC_INFO,
                    reinterpret_cast<host_info_t>(&info), &count) != KERN_SUCCESS)
        throw std::runtime_error("could not query physical memory");
      std::cout << info.max_mem << '\n';
#elif defined(__linux__)
      const auto pages = sysconf(_SC_PHYS_PAGES), size = sysconf(_SC_PAGESIZE);
      if (pages <= 0 || size <= 0) throw std::runtime_error("could not query physical memory");
      std::cout << static_cast<std::uint64_t>(pages) * size << '\n';
#else
      throw std::runtime_error("pass an explicit memory budget on this platform");
#endif
      return 0;
    }
    const auto o = parse(argc, argv);
    if (o.weighting == "weighted") {
      using W = gl::weighted<double>;
      return o.representation == "csr" ? run<gl::csr_graph<gl::undirected, W>>(o) : run<gl::matrix_graph<gl::undirected, W>>(o);
    }
    return o.representation == "csr" ? run<gl::csr_graph<gl::undirected, gl::unweighted>>(o) : run<gl::matrix_graph<gl::undirected, gl::unweighted>>(o);
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
