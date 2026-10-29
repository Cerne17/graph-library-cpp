// Part 2 benchmark: weighted graphs on the new library (include/graph/).
// Driven by analysis/harness_weighted.py; see usage() for the command line.
//
// Output on stdout, one CSV row per line, first field tells the row kind:
//   info,<metric>,<value>
//   time,<algorithm>,<average_us>,<median_us>,<samples>,<status>
//   path,<source>,<target>,<source_name>,<target_name>,<distance>,<hops>,
//        <path>,<path_names>,<status>
// Statuses: ok, not_implemented (negative weight), skipped, mismatch,
// unreachable, out_of_range, unknown_name.

#include "graph/algorithms/dijkstra.hpp"
#include "graph/algorithms/path.hpp"
#include "graph/graph.hpp"
#include "graph/io/edge_list.hpp"
#include "graph/io/label_map.hpp"
#include "graph/policies.hpp"
#include "graph/types.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#ifdef __GLIBC__
#include <malloc.h> // malloc_trim
#endif

namespace {

using weighting = gl::weighted<double>;
using csr_type = gl::csr_graph<gl::undirected, weighting>;
using matrix_type = gl::matrix_graph<gl::undirected, weighting>;

struct options
{
  std::string graph_file;
  std::string representation; // csr | matrix
  std::string mode;           // mem | time | paths | all
  std::size_t k = 100;
  std::uint32_t seed = 2025;
  bool run_array = true;
  std::string labels_file; // empty: --source/--target are vertex ids
  std::string source;
  std::vector<std::string> targets;
};

void usage()
{
  std::cerr
    << "usage: benchmark_weighted <graph> <csr|matrix> <mem|time|paths|all>\n"
       "         [--k N] [--seed S] [--array yes|no]\n"
       "         [--labels FILE] [--source S] [--target T]...\n"
       "  --source/--target are vertex ids, or names when --labels is given\n";
}

long long parse_integer(std::string_view text, std::string_view what)
{
  long long value = 0;
  const auto [end, ec] =
    std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || end != text.data() + text.size()) {
    throw std::invalid_argument(std::string(what) + " is not an integer: " +
                                std::string(text));
  }
  return value;
}

options parse_options(int argc, char* argv[])
{
  if (argc < 4) {
    usage();
    throw std::invalid_argument("missing arguments");
  }
  options opt;
  opt.graph_file = argv[1];
  opt.representation = argv[2];
  opt.mode = argv[3];

  for (int i = 4; i < argc; ++i) {
    const std::string_view flag = argv[i];
    if (i + 1 >= argc) {
      throw std::invalid_argument("missing value for " + std::string(flag));
    }
    const std::string value = argv[++i];

    if (flag == "--k") {
      const long long k = parse_integer(value, "--k");
      if (k < 1) {
        throw std::invalid_argument("--k must be positive");
      }
      opt.k = static_cast<std::size_t>(k);
    } else if (flag == "--seed") {
      const long long seed = parse_integer(value, "--seed");
      if (seed < 0 || seed > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("--seed out of range");
      }
      opt.seed = static_cast<std::uint32_t>(seed);
    } else if (flag == "--array") {
      if (value != "yes" && value != "no") {
        throw std::invalid_argument("--array must be yes or no");
      }
      opt.run_array = value == "yes";
    } else if (flag == "--labels") {
      opt.labels_file = value;
    } else if (flag == "--source") {
      opt.source = value;
    } else if (flag == "--target") {
      opt.targets.push_back(value);
    } else {
      throw std::invalid_argument("unknown option " + std::string(flag));
    }
  }

  if (opt.representation != "csr" && opt.representation != "matrix") {
    throw std::invalid_argument("invalid representation: " +
                                opt.representation);
  }
  return opt;
}

// A CSV field, quoted when it holds a comma, a quote or a newline.
std::string csv_field(std::string_view s)
{
  if (s.find_first_of(",\"\n") == std::string_view::npos) {
    return std::string(s);
  }
  std::string out = "\"";
  for (const char c : s) {
    if (c == '"') {
      out += '"';
    }
    out += c;
  }
  out += '"';
  return out;
}

// Shortest round-trip representation; "inf" for +infinity.
std::string format_number(double x)
{
  return std::format("{}", x);
}

template<typename G>
struct loaded_graph
{
  G graph;
  std::size_t edges;
  std::size_t skipped_self_loops;
};

// Reads the file and builds G, then frees the edge list so that the RSS seen
// at READY is (as far as the allocator allows) the graph alone.
template<typename G>
loaded_graph<G> load(const std::string& path)
{
  std::optional<G> graph;
  std::size_t edges = 0;
  std::size_t skipped = 0;
  {
    const auto list = gl::io::read_edge_list<weighting>(path);
    graph.emplace(G::from_edges(list.n, list.edges));
    edges = list.edges.size();
    skipped = list.skipped_self_loops;
  }
#ifdef __GLIBC__
  malloc_trim(0); // glibc keeps freed pages otherwise, inflating the RSS
#endif
  return { std::move(*graph), edges, skipped };
}

// --- mem: signal ready, block until released, so an external observer can
//     snapshot this process's RSS while it holds the graph. ---
void hold_for_memory()
{
  std::cout << "READY\n";
  std::cout.flush();
  std::string dummy;
  std::getline(std::cin, dummy);
}

// {average, median} of a sample, sorted in place.
std::pair<double, double> summarize(std::vector<double>& samples)
{
  double sum = 0;
  for (const double s : samples) {
    sum += s;
  }
  const double average = sum / static_cast<double>(samples.size());

  std::ranges::sort(samples);
  const std::size_t mid = samples.size() / 2;
  const double median = samples.size() % 2 == 0
                          ? (samples[mid - 1] + samples[mid]) / 2.0
                          : samples[mid];
  return { average, median };
}

// k distinct vertices drawn uniformly from 1..n with a fixed seed, in draw
// order. All of 1..n when k >= n.
std::vector<gl::vertex_id> sample_sources(gl::vertex_id n,
                                          std::size_t k,
                                          std::uint32_t seed)
{
  std::vector<gl::vertex_id> sources;
  if (k >= n) {
    for (gl::vertex_id v = 1; v <= n; ++v) {
      sources.push_back(v);
    }
    return sources;
  }

  std::mt19937 rng(seed);
  std::uniform_int_distribution<gl::vertex_id> pick(1, n);
  std::unordered_set<gl::vertex_id> chosen;
  while (sources.size() < k) {
    const gl::vertex_id v = pick(rng);
    if (chosen.insert(v).second) {
      sources.push_back(v);
    }
  }
  return sources;
}

// Same distances up to rounding: equal-length paths found in a different
// order may sum their weights in a different order.
bool same_distances(const std::vector<double>& a, const std::vector<double>& b)
{
  return std::ranges::equal(a, b, [](double x, double y) {
    if (std::isinf(x) || std::isinf(y)) {
      return x == y;
    }
    return std::abs(x - y) <= 1e-9 * std::max({ 1.0, std::abs(x), std::abs(y) });
  });
}

void print_time_row(std::string_view algorithm,
                    std::vector<double>& samples,
                    std::string_view status)
{
  if (samples.empty()) {
    std::cout << "time," << algorithm << ",,,0," << status << "\n";
    return;
  }
  const auto [average, median] = summarize(samples);
  std::cout << "time," << algorithm << "," << format_number(average) << ","
            << format_number(median) << "," << samples.size() << "," << status
            << "\n";
}

// --- time: the Dijkstra versions over the same k random sources. Each run
//     is timed on its own (allocation of dist/parent included); distances are
//     compared against dijkstra_heap outside the timed region. ---
template<typename G>
void run_timing(const G& g, const options& opt)
{
  using clock = std::chrono::steady_clock;
  using micros = std::chrono::duration<double, std::micro>;

  const auto sources = sample_sources(g.vertex_count(), opt.k, opt.seed);
  std::cout << "info,seed," << opt.seed << "\n";
  std::cout << "info,k," << sources.size() << "\n";

  std::vector<double> array_us;
  std::vector<double> heap_us;
  std::vector<double> lazy_us;
  bool array_matches = true;
  bool lazy_matches = true;
  double sink = 0;

  try {
    for (const gl::vertex_id s : sources) {
      auto t0 = clock::now();
      const auto heap = gl::algorithms::dijkstra_heap(g, s);
      auto t1 = clock::now();
      heap_us.push_back(micros(t1 - t0).count());
      sink += heap.dist[g.vertex_count()];

      t0 = clock::now();
      const auto lazy = gl::algorithms::dijkstra_lazy(g, s);
      t1 = clock::now();
      lazy_us.push_back(micros(t1 - t0).count());
      lazy_matches = lazy_matches && same_distances(heap.dist, lazy.dist);

      if (opt.run_array) {
        t0 = clock::now();
        const auto array = gl::algorithms::dijkstra_array(g, s);
        t1 = clock::now();
        array_us.push_back(micros(t1 - t0).count());
        array_matches = array_matches && same_distances(heap.dist, array.dist);
      }
    }
  } catch (const std::invalid_argument& e) {
    std::cerr << "time: " << e.what() << "\n";
    for (const auto algorithm :
         { "dijkstra_array", "dijkstra_heap", "dijkstra_lazy" }) {
      std::cout << "time," << algorithm << ",,,0,not_implemented\n";
    }
    return;
  }
  std::cerr << "sink=" << sink << "\n";

  print_time_row("dijkstra_array",
                 array_us,
                 !opt.run_array ? "skipped"
                 : array_matches ? "ok"
                                 : "mismatch");
  print_time_row("dijkstra_heap", heap_us, "ok");
  print_time_row("dijkstra_lazy", lazy_us, lazy_matches ? "ok" : "mismatch");
}

void print_path_row(std::string_view source,
                    std::string_view target,
                    std::string_view source_name,
                    std::string_view target_name,
                    std::string_view distance,
                    std::string_view hops,
                    std::string_view path,
                    std::string_view path_names,
                    std::string_view status)
{
  std::cout << "path," << csv_field(source) << "," << csv_field(target) << ","
            << csv_field(source_name) << "," << csv_field(target_name) << ","
            << distance << "," << hops << "," << csv_field(path) << ","
            << csv_field(path_names) << "," << status << "\n";
}

// A vertex given on the command line: an id, or a name looked up in labels.
// Empty when it does not exist (id out of 1..n, or unknown name).
std::optional<gl::vertex_id> resolve(const std::string& text,
                                     gl::vertex_id n,
                                     const gl::io::label_map* labels)
{
  if (labels != nullptr) {
    try {
      return labels->id_of(text);
    } catch (const std::invalid_argument&) {
      return std::nullopt;
    }
  }
  const long long id = parse_integer(text, "vertex id");
  if (id < 1 || id > n) {
    return std::nullopt;
  }
  return static_cast<gl::vertex_id>(id);
}

// --- paths: one dijkstra_heap run from the source, then distance and path
//     to each target. ---
template<typename G>
void run_paths(const G& g, const options& opt)
{
  if (opt.source.empty() || opt.targets.empty()) {
    throw std::invalid_argument("paths: --source and --target are required");
  }

  std::optional<gl::io::label_map> labels;
  if (!opt.labels_file.empty()) {
    labels.emplace(gl::io::read_label_map(opt.labels_file));
    if (labels->size() != g.vertex_count()) {
      throw std::runtime_error(
        std::format("paths: {} has {} names for {} vertices",
                    opt.labels_file,
                    labels->size(),
                    g.vertex_count()));
    }
  }
  const gl::io::label_map* names = labels ? &*labels : nullptr;
  const bool by_name = names != nullptr;

  // Row fields for a vertex: (id column, name column).
  const auto id_text = [by_name](std::optional<gl::vertex_id> v,
                                 const std::string& given) {
    return v ? std::to_string(*v) : (by_name ? std::string() : given);
  };
  const auto name_text = [by_name](const std::string& given) {
    return by_name ? given : std::string();
  };

  const auto source = resolve(opt.source, g.vertex_count(), names);
  const std::string source_id = id_text(source, opt.source);

  if (!source) {
    const char* status = by_name ? "unknown_name" : "out_of_range";
    for (const auto& t : opt.targets) {
      const auto target = resolve(t, g.vertex_count(), names);
      print_path_row(source_id, id_text(target, t),
                     name_text(opt.source), name_text(t), "", "", "", "",
                     status);
    }
    return;
  }

  std::optional<gl::algorithms::shortest_paths<double>> sp;
  try {
    sp = gl::algorithms::dijkstra_heap(g, *source);
  } catch (const std::invalid_argument& e) {
    std::cerr << "paths: " << e.what() << "\n";
  }

  for (const auto& t : opt.targets) {
    const auto target = resolve(t, g.vertex_count(), names);
    const std::string target_id = id_text(target, t);

    if (!target) {
      print_path_row(source_id, target_id, name_text(opt.source), name_text(t),
                     "", "", "", "", by_name ? "unknown_name" : "out_of_range");
      continue;
    }
    if (!sp) {
      print_path_row(source_id, target_id, name_text(opt.source), name_text(t),
                     "", "", "", "", "not_implemented");
      continue;
    }

    const double d = sp->dist[*target];
    if (std::isinf(d)) {
      print_path_row(source_id, target_id, name_text(opt.source), name_text(t),
                     format_number(d), "", "", "", "unreachable");
      continue;
    }

    const auto path = gl::algorithms::reconstruct_path(*sp, *target);
    std::string path_ids;
    std::string path_names;
    for (const gl::vertex_id v : path) {
      if (!path_ids.empty()) {
        path_ids += ' ';
        if (by_name) {
          path_names += " | ";
        }
      }
      path_ids += std::to_string(v);
      if (by_name) {
        path_names += names->name_of(v);
      }
    }
    print_path_row(source_id, target_id, name_text(opt.source), name_text(t),
                   format_number(d), std::to_string(path.size() - 1), path_ids,
                   path_names, "ok");
  }
}

template<typename G>
int run(const options& opt)
{
  const auto loaded = load<G>(opt.graph_file);
  const auto print_info = [&] {
    std::cout << "info,vertices," << loaded.graph.vertex_count() << "\n";
    std::cout << "info,edges," << loaded.edges << "\n";
    std::cout << "info,skipped_self_loops," << loaded.skipped_self_loops
              << "\n";
  };

  if (opt.mode == "mem") {
    hold_for_memory();
    print_info();
  } else if (opt.mode == "time") {
    print_info();
    run_timing(loaded.graph, opt);
  } else if (opt.mode == "paths") {
    print_info();
    run_paths(loaded.graph, opt);
  } else if (opt.mode == "all") {
    // Memory first: snapshot the pristine graph before the algorithms
    // allocate their own structures.
    hold_for_memory();
    print_info();
    run_timing(loaded.graph, opt);
    if (!opt.source.empty()) {
      run_paths(loaded.graph, opt);
    }
  } else {
    usage();
    std::cerr << "unknown mode: " << opt.mode << "\n";
    return 1;
  }
  return 0;
}

} // namespace

int main(int argc, char* argv[])
{
  try {
    const options opt = parse_options(argc, argv);
    return opt.representation == "csr" ? run<csr_type>(opt)
                                       : run<matrix_type>(opt);
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
