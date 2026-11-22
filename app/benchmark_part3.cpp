// Part 3 benchmark: directed weighted graphs, Bellman-Ford toward a target,
// the whole-graph negative-cycle check and Floyd-Warshall, on the new library
// (include/graph/). Driven by analysis/harness_part3.py; see usage().
//
// Weights are read as text with 2 decimals and scaled by 100 into exact
// integer-valued doubles (fixed_point_weight); distances are printed unscaled
// with exactly 2 decimals. Disk I/O is never timed.
//
// Output on stdout, one CSV row per line, flushed after every row:
//   info,<metric>,<value>
//   time,<algorithm>,<run>,<micros>[,<param>]
//   calib,floyd_warshall,<n>,<arcs>,<run>,<micros>
//   fit,floyd_warshall_n3,<seconds per n^3>,<points>
//   progress,<algorithm>,<k>,<elapsed_micros>,<rows_not_skipped>
//   path,<algorithm>,<source>,<target>,<distance>,<hops>,<path>,<status>
//   cycle,<found>,<length>,<weight>,<vertices>,<status>
//   check,<name>,<status>,<detail>
//   done,<mode>
// Path statuses: ok, unreachable, negative_cycle, out_of_range.

#include "graph/algorithms/bellman_ford.hpp"
#include "graph/algorithms/floyd_warshall.hpp"
#include "graph/algorithms/path.hpp"
#include "graph/graph.hpp"
#include "graph/io/edge_list.hpp"
#include "graph/io/fixed_point.hpp"
#include "graph/policies.hpp"
#include "graph/storage/reversed.hpp"
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
#include <numeric>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using weighting = gl::weighted<double>;
using wedge = gl::weighted_edge<double>;

// The one named constant of the fixed-point convention; the scale comes from it.
constexpr unsigned weight_decimals = 2;
const gl::io::fixed_point_weight<double> weight_parser{ weight_decimals };
const double scale = weight_parser.scale();

constexpr double inf = std::numeric_limits<double>::infinity();

using clock_type = std::chrono::steady_clock;

// Results the timing loops would otherwise discard; printed to stderr at the
// end so the compiler cannot drop the timed work.
double sink = 0;

double micros_since(clock_type::time_point start)
{
  return std::chrono::duration<double, std::micro>(clock_type::now() - start)
    .count();
}

struct options
{
  std::string graph_file;
  std::string mode; // calibrate | cycle | bf | fw | allpairs
  std::optional<bool> directed;
  std::string target;
  std::vector<std::string> sources;
  std::vector<std::string> set;
  std::size_t runs = 10;
  double budget_seconds = 600;
  std::vector<std::string> optimizations;
  std::uint32_t progress_every = 250;
  std::size_t k = 100;
  std::uint64_t seed = 2025;
  bool no_bf_pass = false;
};

void usage()
{
  std::cerr
    << "usage: benchmark_part3 <graph> --directed yes|no "
       "<calibrate|cycle|bf|fw|allpairs>\n"
       "         [--target T] [--sources a,b,c] [--set a,b,c,...]\n"
       "         [--runs N] [--budget-seconds S] "
       "[--optimizations textbook,stop_when_stable,frontier]\n"
       "         [--progress-every K] [--k N] [--seed S] [--no-bf-pass]\n";
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

std::vector<std::string> split_list(std::string_view text)
{
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t comma = text.find(',', start);
    const std::size_t end = comma == std::string_view::npos ? text.size() : comma;
    if (end > start) {
      out.emplace_back(text.substr(start, end - start));
    }
    if (comma == std::string_view::npos) {
      break;
    }
    start = comma + 1;
  }
  return out;
}

options parse_options(int argc, char* argv[])
{
  if (argc < 2) {
    usage();
    throw std::invalid_argument("missing arguments");
  }
  options opt;
  opt.graph_file = argv[1];

  for (int i = 2; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (!arg.starts_with("--")) {
      if (!opt.mode.empty()) {
        throw std::invalid_argument("two modes given: " + opt.mode + " and " +
                                    std::string(arg));
      }
      opt.mode = arg;
      continue;
    }
    if (arg == "--no-bf-pass") {
      opt.no_bf_pass = true;
      continue;
    }
    if (i + 1 >= argc) {
      throw std::invalid_argument("missing value for " + std::string(arg));
    }
    const std::string value = argv[++i];
    if (arg == "--directed") {
      if (value != "yes" && value != "no") {
        throw std::invalid_argument("--directed must be yes or no");
      }
      opt.directed = value == "yes";
    } else if (arg == "--target") {
      opt.target = value;
    } else if (arg == "--sources") {
      opt.sources = split_list(value);
    } else if (arg == "--set") {
      opt.set = split_list(value);
    } else if (arg == "--runs") {
      const long long runs = parse_integer(value, "--runs");
      if (runs < 1) {
        throw std::invalid_argument("--runs must be positive");
      }
      opt.runs = static_cast<std::size_t>(runs);
    } else if (arg == "--budget-seconds") {
      const long long seconds = parse_integer(value, "--budget-seconds");
      if (seconds < 0) {
        throw std::invalid_argument("--budget-seconds must not be negative");
      }
      opt.budget_seconds = static_cast<double>(seconds);
    } else if (arg == "--optimizations") {
      opt.optimizations = split_list(value);
    } else if (arg == "--progress-every") {
      const long long every = parse_integer(value, "--progress-every");
      if (every < 1) {
        throw std::invalid_argument("--progress-every must be positive");
      }
      opt.progress_every = static_cast<std::uint32_t>(every);
    } else if (arg == "--k") {
      const long long k = parse_integer(value, "--k");
      if (k < 1) {
        throw std::invalid_argument("--k must be positive");
      }
      opt.k = static_cast<std::size_t>(k);
    } else if (arg == "--seed") {
      const long long seed = parse_integer(value, "--seed");
      if (seed < 0) {
        throw std::invalid_argument("--seed must not be negative");
      }
      opt.seed = static_cast<std::uint64_t>(seed);
    } else {
      throw std::invalid_argument("unknown option " + std::string(arg));
    }
  }

  if (!opt.directed) {
    usage();
    throw std::invalid_argument("--directed yes|no is required");
  }
  if (opt.mode != "calibrate" && opt.mode != "cycle" && opt.mode != "bf" &&
      opt.mode != "fw" && opt.mode != "allpairs") {
    usage();
    throw std::invalid_argument("unknown or missing mode: " + opt.mode);
  }
  return opt;
}

gl::algorithms::bellman_ford_variant variant_named(const std::string& name)
{
  using gl::algorithms::bellman_ford_variant;
  if (name == "textbook") {
    return bellman_ford_variant::textbook;
  }
  if (name == "stop_when_stable") {
    return bellman_ford_variant::stop_when_stable;
  }
  if (name == "frontier") {
    return bellman_ford_variant::frontier;
  }
  throw std::invalid_argument("unknown variant: " + name);
}

// ---- output -----------------------------------------------------------------

void emit(const std::string& row)
{
  std::cout << row << '\n';
  std::cout.flush();
}

std::string format_number(double x)
{
  return std::format("{}", x);
}

// A scaled distance, unscaled with exactly 2 decimals; inf and -inf as such.
std::string format_distance(double scaled)
{
  if (scaled == inf) {
    return "inf";
  }
  if (scaled == -inf) {
    return "-inf";
  }
  return std::format("{:.2f}", scaled / scale);
}

std::string join_ids(const std::vector<gl::vertex_id>& path)
{
  std::string out;
  for (const gl::vertex_id v : path) {
    if (!out.empty()) {
      out += ' ';
    }
    out += std::to_string(v);
  }
  return out;
}

struct path_row
{
  std::string algorithm;
  std::string source;
  std::string target;
  std::string distance;
  std::string hops;
  std::string path;
  std::string status;
};

void emit_path(const path_row& r)
{
  emit("path," + r.algorithm + "," + r.source + "," + r.target + "," +
       r.distance + "," + r.hops + "," + r.path + "," + r.status);
}

// A vertex id given on the command line, or empty when it is not in 1..n.
std::optional<gl::vertex_id> resolve(const std::string& text, gl::vertex_id n)
{
  const long long id = parse_integer(text, "vertex id");
  if (id < 1 || id > n) {
    return std::nullopt;
  }
  return static_cast<gl::vertex_id>(id);
}

// ---- the budget rule --------------------------------------------------------

// Run 1 always runs. Another run starts while runs < max_runs and elapsed
// (including run 1) + the longest run so far <= budget. run_once returns its
// own time in microseconds and prints its time row.
template<typename RunOnce>
void repeat_within_budget(std::size_t max_runs, double budget_seconds, RunOnce run_once)
{
  const double budget = budget_seconds * 1e6;
  double elapsed = 0;
  double longest = 0;
  for (std::size_t run = 1; run <= max_runs; ++run) {
    if (run > 1 && elapsed + longest > budget) {
      break;
    }
    const double micros = run_once(run);
    elapsed += micros;
    longest = std::max(longest, micros);
  }
}

void emit_time(std::string_view algorithm,
               std::size_t run,
               double micros,
               const std::string& param = {})
{
  std::string row = "time," + std::string(algorithm) + "," + std::to_string(run) +
                    "," + format_number(micros);
  if (!param.empty()) {
    row += "," + param;
  }
  emit(row);
}

// ---- loading ----------------------------------------------------------------

struct loaded
{
  gl::vertex_id n = 0;
  std::vector<wedge> edges;
  std::vector<wedge> negative_loops;
  std::size_t skipped_self_loops = 0;
};

loaded load(const std::string& path)
{
  auto list = gl::io::read_edge_list<weighting>(path, weight_parser);
  return { list.n,
           std::move(list.edges),
           std::move(list.negative_self_loop_edges),
           list.skipped_self_loops };
}

// Sums of scaled weights stay exact while (n - 1) * max |w| <= 2^53.
void check_exactness(const loaded& in)
{
  double max_abs = 0;
  for (const auto& e : in.edges) {
    max_abs = std::max(max_abs, std::abs(e.weight));
  }
  for (const auto& e : in.negative_loops) {
    max_abs = std::max(max_abs, std::abs(e.weight));
  }
  emit("info,max_abs_weight," + format_distance(max_abs));
  const double bound = 9007199254740992.0; // 2^53
  if (in.n > 1 && static_cast<double>(in.n - 1) * max_abs > bound) {
    throw std::runtime_error(
      "path sums could exceed 2^53 and lose exactness: (n - 1) * max |w| = " +
      format_number(static_cast<double>(in.n - 1) * max_abs / scale));
  }
}

// ---- modes --------------------------------------------------------------------

template<typename G>
void run_cycle(const G& g, const loaded& in, const options& opt)
{
  gl::algorithms::negative_cycle_report report;
  repeat_within_budget(opt.runs, opt.budget_seconds, [&](std::size_t run) {
    const auto start = clock_type::now();
    report = gl::algorithms::find_negative_cycle(g, std::span<const wedge>(in.negative_loops));
    const double micros = micros_since(start);
    emit_time("negative_cycle_check", run, micros);
    return micros;
  });

  if (!report.found) {
    emit("cycle,no,0,,,ok");
    emit("check,witness,ok,no cycle");
    return;
  }

  // the witness must be made of arcs (or a negative self-loop) and weigh < 0
  const auto lightest = [&](gl::vertex_id from, gl::vertex_id to) {
    double best = inf;
    if (from == to) {
      for (const auto& loop : in.negative_loops) {
        if (loop.u == from) {
          best = std::min(best, loop.weight);
        }
      }
      return best;
    }
    g.for_each_edge(from, [&](gl::vertex_id v, double w) {
      if (v == to) {
        best = std::min(best, w);
      }
    });
    return best;
  };
  const auto& cycle = report.cycle;
  double total = 0;
  bool arcs_exist = !cycle.empty();
  for (std::size_t i = 0; i < cycle.size(); ++i) {
    const double w = lightest(cycle[i], cycle[(i + 1) % cycle.size()]);
    if (w == inf) {
      arcs_exist = false;
    }
    total += w;
  }
  const bool valid = arcs_exist && total < 0;
  const std::string status = valid ? "ok" : "error";
  emit("cycle,yes," + std::to_string(cycle.size()) + "," +
       (arcs_exist ? format_distance(total) : std::string("")) + "," +
       join_ids(cycle) + "," + status);
  emit("check,witness," + status + "," +
       (valid ? std::string("arcs exist; weight < 0")
              : std::string("witness is not a negative cycle")));
}

// Bellman-Ford toward target on rev (the reversal, or the undirected graph).
template<typename R>
auto bf_pass(const R& rev,
             gl::vertex_id target,
             const loaded& in,
             gl::algorithms::bellman_ford_variant variant,
             gl::algorithms::bellman_ford_stats* stats)
{
  return gl::algorithms::bellman_ford_on_reversed(
    rev, target, std::span<const wedge>(in.negative_loops), { variant }, stats);
}

template<typename G>
void run_bf(const G& g, const loaded& in, const options& opt)
{
  using gl::algorithms::bellman_ford_variant;
  if (opt.target.empty() || opt.sources.empty()) {
    throw std::invalid_argument("bf: --target and --sources are required");
  }
  constexpr bool directed = G::direction_type::is_directed;
  const auto target = resolve(opt.target, in.n);
  if (!target) {
    for (const auto& s : opt.sources) {
      emit_path({ "bellman_ford", s, opt.target, "", "", "", "out_of_range" });
    }
    return;
  }

  std::optional<gl::algorithms::target_paths<double>> answer;
  gl::algorithms::bellman_ford_stats answer_stats;

  // one run = the reversal (directed only) + one pass toward the target
  repeat_within_budget(opt.runs, opt.budget_seconds, [&](std::size_t run) {
    gl::algorithms::bellman_ford_stats stats;
    double reverse_micros = 0;
    double pass_micros = 0;
    if constexpr (directed) {
      const auto start = clock_type::now();
      const auto rev = gl::reversed(g);
      reverse_micros = micros_since(start);
      const auto pass_start = clock_type::now();
      auto t = bf_pass(rev, *target, in, bellman_ford_variant::frontier, &stats);
      pass_micros = micros_since(pass_start);
      if (!answer) {
        answer = std::move(t);
      }
      emit_time("reverse", run, reverse_micros);
    } else {
      const auto pass_start = clock_type::now();
      auto t = bf_pass(g, *target, in, bellman_ford_variant::frontier, &stats);
      pass_micros = micros_since(pass_start);
      if (!answer) {
        answer = std::move(t);
      }
    }
    if (run == 1) {
      answer_stats = stats;
    }
    emit_time("bellman_ford_pass", run, pass_micros);
    return reverse_micros + pass_micros;
  });

  emit("info,rounds," + std::to_string(answer_stats.rounds));
  emit("info,relaxations," + std::to_string(answer_stats.relaxations));
  emit("info,cycle_reaches_target," +
       std::string(answer->cycle_reaches_target ? "yes" : "no"));

  for (const auto& s : opt.sources) {
    path_row row{ "bellman_ford", s, std::to_string(*target), "", "", "", "" };
    const auto source = resolve(s, in.n);
    if (!source) {
      row.status = "out_of_range";
    } else {
      const double d = answer->dist[*source];
      row.distance = format_distance(d);
      if (d == inf) {
        row.status = "unreachable";
      } else if (d == -inf) {
        row.status = "negative_cycle";
      } else {
        const auto path = gl::algorithms::path_to_target(*answer, *source);
        row.hops = std::to_string(path.size() - 1);
        row.path = join_ids(path);
        row.status = "ok";
      }
    }
    emit_path(row);
  }

  // the optimization comparison: passes only, on one reversal
  if (!opt.optimizations.empty()) {
    const auto compare = [&](const auto& rev) {
      for (const auto& name : opt.optimizations) {
        const auto variant = variant_named(name);
        gl::algorithms::bellman_ford_stats first;
        repeat_within_budget(opt.runs, opt.budget_seconds, [&](std::size_t run) {
          gl::algorithms::bellman_ford_stats stats;
          const auto start = clock_type::now();
          const auto t = bf_pass(rev, *target, in, variant, &stats);
          const double micros = micros_since(start);
          sink += t.dist[*target];
          if (run == 1) {
            first = stats;
          }
          emit_time("bellman_ford_pass_" + name, run, micros);
          return micros;
        });
        emit("info,rounds_" + name + "," + std::to_string(first.rounds));
        emit("info,relaxations_" + name + "," + std::to_string(first.relaxations));
      }
    };
    if constexpr (directed) {
      compare(gl::reversed(g));
    } else {
      compare(g);
    }
  }
}

template<typename G>
void run_fw(const G& g, const loaded& in, const options& opt)
{
  if (opt.set.empty()) {
    throw std::invalid_argument("fw: --set is required");
  }
  std::optional<gl::algorithms::all_pairs<double>> answer;

  repeat_within_budget(opt.runs, opt.budget_seconds, [&](std::size_t run) {
    const auto start = clock_type::now();
    auto last_print = start;
    gl::vertex_id since_print = 0;
    const auto on_pivot = [&](gl::vertex_id k, gl::vertex_id rows) {
      ++since_print;
      const auto now = clock_type::now();
      if (since_print >= opt.progress_every ||
          now - last_print >= std::chrono::seconds(60) || k == in.n) {
        emit("progress,floyd_warshall," + std::to_string(k) + "," +
             format_number(micros_since(start)) + "," + std::to_string(rows));
        last_print = now;
        since_print = 0;
      }
    };
    auto r = gl::algorithms::floyd_warshall(
      g, std::span<const wedge>(in.negative_loops), on_pivot);
    const double micros = micros_since(start);
    emit_time("floyd_warshall", run, micros);
    if (!answer) {
      answer = std::move(r);
    }
    return micros;
  });

  emit("info,memory_bytes," + std::to_string(answer->bytes()));
  emit("info,negative_cycle," + std::string(answer->negative_cycle() ? "yes" : "no"));

  for (const auto& si : opt.set) {
    for (const auto& sj : opt.set) {
      if (si == sj) {
        continue;
      }
      path_row row{ "floyd_warshall", si, sj, "", "", "", "" };
      const auto i = resolve(si, in.n);
      const auto j = resolve(sj, in.n);
      if (!i || !j) {
        row.status = "out_of_range";
      } else if (answer->minus_infinity(*i, *j)) {
        row.distance = "-inf";
        row.status = "negative_cycle";
      } else if (answer->dist(*i, *j) == inf) {
        row.distance = "inf";
        row.status = "unreachable";
      } else {
        const auto path =
          gl::algorithms::reconstruct_path(answer->tree_from(*i), *j);
        row.distance = format_distance(answer->dist(*i, *j));
        row.hops = std::to_string(path.size() - 1);
        row.path = join_ids(path);
        row.status = "ok";
      }
      emit_path(row);
    }
  }
}

template<typename G>
void run_allpairs(const G& g, const loaded& in, const options& opt)
{
  using gl::algorithms::bellman_ford_variant;
  constexpr bool directed = G::direction_type::is_directed;
  const double budget = opt.budget_seconds * 1e6;

  std::vector<gl::vertex_id> order(in.n);
  std::iota(order.begin(), order.end(), gl::vertex_id{ 1 });
  std::mt19937_64 rng(opt.seed);
  std::ranges::shuffle(order, rng);

  const auto passes = [&](const auto& rev, double reverse_micros) {
    double elapsed = reverse_micros;
    double longest = 0;
    bool full = false;
    std::size_t done = 0;
    for (const gl::vertex_id t : order) {
      if (done > 0) {
        if (!full && (done >= opt.k || elapsed + longest > budget)) {
          break;
        }
      }
      const auto start = clock_type::now();
      const auto result = bf_pass(rev, t, in, bellman_ford_variant::frontier, nullptr);
      const double micros = micros_since(start);
      sink += result.dist[order.front()];
      emit_time("allpairs_pass", done + 1, micros, std::to_string(t));
      elapsed += micros;
      longest = std::max(longest, micros);
      ++done;
      if (!full && reverse_micros + static_cast<double>(in.n) * longest <= budget) {
        full = true;
      }
    }
    emit("info,targets_measured," + std::to_string(done));
    emit("info,allpairs_full," + std::string(done == in.n ? "yes" : "no"));
  };

  if constexpr (directed) {
    const auto start = clock_type::now();
    const auto rev = gl::reversed(g);
    const double reverse_micros = micros_since(start);
    emit_time("reverse", 1, reverse_micros);
    passes(rev, reverse_micros);
  } else {
    passes(g, 0.0);
  }
}

// A random directed graph with the given average out-degree and integer
// scaled weights 0..1000.
gl::csr_graph<gl::directed, weighting> calibration_graph(gl::vertex_id n,
                                                       double degree,
                                                       std::mt19937_64& rng)
{
  std::uniform_int_distribution<gl::vertex_id> pick(1, n);
  std::uniform_int_distribution<int> weight(0, 1000);
  const auto m = static_cast<std::size_t>(degree * n);
  std::vector<wedge> arcs;
  arcs.reserve(m);
  while (arcs.size() < m) {
    const gl::vertex_id u = pick(rng);
    const gl::vertex_id v = pick(rng);
    if (u != v) {
      arcs.push_back({ u, v, static_cast<double>(weight(rng)) });
    }
  }
  return gl::csr_graph<gl::directed, weighting>::from_edges(n, arcs);
}

template<typename G>
void run_calibrate(const G& g, const loaded& in, const options& opt)
{
  const double degree =
    in.n == 0 ? 0.0 : static_cast<double>(in.edges.size()) / in.n;
  const double budget = opt.budget_seconds * 1e6;
  std::mt19937_64 rng(opt.seed);
  const auto start = clock_type::now();

  double sum_tn3 = 0;
  double sum_n6 = 0;
  std::size_t points = 0;
  double last = 0;
  for (gl::vertex_id n = 500; n <= std::max<gl::vertex_id>(in.n, 500); n *= 2) {
    if (points > 0 && 8 * last > budget) {
      break;
    }
    const auto graph = calibration_graph(n, degree, rng);
    const auto run_start = clock_type::now();
    auto last_print = run_start;
    // a progress row at least every 60 s, so a long size is not mistaken for
    // a hung launch
    const auto on_pivot = [&](gl::vertex_id k, gl::vertex_id rows) {
      const auto now = clock_type::now();
      if (now - last_print >= std::chrono::seconds(60)) {
        emit("progress,calibrate," + std::to_string(k) + "," +
             format_number(micros_since(start)) + "," + std::to_string(rows));
        last_print = now;
      }
    };
    const auto r = gl::algorithms::floyd_warshall(graph, {}, on_pivot);
    last = micros_since(run_start);
    sink += r.dist(1, 1);
    emit("calib,floyd_warshall," + std::to_string(n) + "," +
         std::to_string(static_cast<std::size_t>(degree * n)) + ",1," +
         format_number(last));
    emit("progress,calibrate," + std::to_string(n) + "," +
         format_number(micros_since(start)) + ",0");
    const double n3 = std::pow(static_cast<double>(n), 3);
    sum_tn3 += (last / 1e6) * n3;
    sum_n6 += n3 * n3;
    ++points;
  }
  // least squares through the origin: time = c * n^3
  emit("fit,floyd_warshall_n3," + format_number(sum_tn3 / sum_n6) + "," +
       std::to_string(points));

  if (!opt.no_bf_pass) {
    if (opt.target.empty()) {
      throw std::invalid_argument("calibrate: --target is required (or --no-bf-pass)");
    }
    const auto target = resolve(opt.target, in.n);
    if (!target) {
      throw std::invalid_argument("calibrate: --target out of range");
    }
    const auto pass_start = clock_type::now();
    const auto t = gl::algorithms::bellman_ford(
      g, *target, std::span<const wedge>(in.negative_loops));
    emit_time("bellman_ford_pass", 1, micros_since(pass_start));
    sink += t.dist[*target];
  }
}

template<typename G>
void run(const options& opt)
{
  const loaded in = load(opt.graph_file);
  const auto g = G::from_edges(in.n, in.edges);

  emit("info,vertices," + std::to_string(in.n));
  emit("info,edges," + std::to_string(in.edges.size()));
  emit("info,directed," + std::string(G::direction_type::is_directed ? "yes" : "no"));
  emit("info,skipped_self_loops," + std::to_string(in.skipped_self_loops));
  emit("info,negative_self_loops," + std::to_string(in.negative_loops.size()));
  emit("info,scale," + format_number(scale));
  emit("info,budget_seconds," + format_number(opt.budget_seconds));
  check_exactness(in);

  if (opt.mode == "cycle") {
    run_cycle(g, in, opt);
  } else if (opt.mode == "bf") {
    run_bf(g, in, opt);
  } else if (opt.mode == "fw") {
    run_fw(g, in, opt);
  } else if (opt.mode == "allpairs") {
    emit("info,seed," + std::to_string(opt.seed));
    emit("info,k," + std::to_string(opt.k));
    run_allpairs(g, in, opt);
  } else {
    run_calibrate(g, in, opt);
  }
  emit("done," + opt.mode);
  std::cerr << "sink=" << sink << "\n";
}

} // namespace

int main(int argc, char* argv[])
{
  try {
    const options opt = parse_options(argc, argv);
    if (*opt.directed) {
      run<gl::csr_graph<gl::directed, weighting>>(opt);
    } else {
      run<gl::csr_graph<gl::undirected, weighting>>(opt);
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
}
