#pragma once

#include "graph/policies.hpp"
#include "graph/storage/input_edge.hpp"
#include "graph/types.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace gl::io {

template<weight_policy W>
struct edge_list
{
  vertex_id n = 0;
  std::vector<input_edge_t<W>> edges;
  std::size_t skipped_self_loops = 0; // "v v" lines, left out of edges
  // Weighted files only: the "v v w" lines with w < 0 among the skipped ones,
  // in file order. Each is a negative cycle that the graph built from edges no
  // longer has; the algorithms that care take them as an extra input.
  std::vector<input_edge_t<W>> negative_self_loop_edges;
};

namespace detail {

[[noreturn]] inline void parse_error(const std::filesystem::path& path,
                                     std::size_t line_no,
                                     const std::string& what)
{
  throw std::runtime_error(path.string() + ":" + std::to_string(line_no) +
                           ": " + what);
}

// True if the line holds nothing but whitespace (including a Windows '\r'),
// or if its first non-whitespace character is '#' (a comment).
inline bool is_skippable(const std::string& line)
{
  const auto first = line.find_first_not_of(" \t\r");
  return first == std::string::npos || line[first] == '#';
}

// After the expected fields: true if anything other than whitespace is left.
inline bool has_leftovers(std::istringstream& ls)
{
  char extra;
  return static_cast<bool>(ls >> extra);
}

// The weight parser of the one-argument read_edge_list: stream extraction of
// the whole token, with the reader's historical messages.
template<typename T>
struct stream_weight
{
  T operator()(std::string_view token) const
  {
    std::istringstream ts{ std::string(token) };
    T w{};
    if (!(ts >> w)) {
      throw std::invalid_argument("expected an edge weight");
    }
    if (has_leftovers(ts)) {
      throw std::invalid_argument("unexpected text after the edge");
    }
    return w;
  }
};

// Stands in for the parser of unweighted policies; never called.
struct no_weight
{};

// The reader shared by both read_edge_list overloads. parse_weight turns the
// weight token into a weight (weighted policies only) or throws
// std::invalid_argument, rethrown as "path:line: <message>".
template<weight_policy W, typename ParseWeight>
edge_list<W> read_edge_list_impl(const std::filesystem::path& path,
                                 ParseWeight& parse_weight);

} // namespace detail

// Reads "n" on the first non-blank line, then one edge per line:
// "u v" for unweighted policies, "u v w" for weighted ones.
// Blank lines and comments (marked with a starting '#') are skipped. Self-loops
// ("v v") are validated like any edge, then dropped and counted in
// skipped_self_loops (and, when their weight is negative, kept in
// negative_self_loop_edges): the storages do not accept them. Any malformed input
// throws std::runtime_error.
template<weight_policy W>
edge_list<W> read_edge_list(const std::filesystem::path& path)
{
  if constexpr (W::is_weighted) {
    detail::stream_weight<typename W::weight_type> parse;
    return detail::read_edge_list_impl<W>(path, parse);
  } else {
    detail::no_weight parse;
    return detail::read_edge_list_impl<W>(path, parse);
  }
}

// Like read_edge_list<W>(path), but the weight field is handed to
// parse_weight as its text (the token between whitespace). parse_weight
// returns the weight, or throws std::invalid_argument with a message; the
// reader rethrows it as std::runtime_error "path:line: <message>".
// W must be given explicitly: it appears only in the return type.
template<weight_policy W, typename ParseWeight>
  requires W::is_weighted &&
           std::is_invocable_r_v<typename W::weight_type,
                                 ParseWeight&,
                                 std::string_view>
edge_list<W> read_edge_list(const std::filesystem::path& path,
                            ParseWeight parse_weight)
{
  return detail::read_edge_list_impl<W>(path, parse_weight);
}

template<weight_policy W, typename ParseWeight>
edge_list<W> detail::read_edge_list_impl(const std::filesystem::path& path,
                                         ParseWeight& parse_weight)
{
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("could not open file " + path.string());
  }

  edge_list<W> result;
  std::string line;
  std::size_t line_no = 0;

  // 1. vertex count: the first non-blank line
  // n + 2 must still fit in a vertex_id (csr_storage allocates n + 2 offsets)
  constexpr long long max_n =
    static_cast<long long>(std::numeric_limits<vertex_id>::max()) - 2;

  bool found_n = false;
  while (std::getline(in, line)) {
    ++line_no;
    if (detail::is_skippable(line)) {
      continue;
    }

    std::istringstream ls(line);
    long long raw_n;
    if (!(ls >> raw_n)) {
      detail::parse_error(path, line_no, "expected the vertex count");
    }
    if (raw_n < 0 || raw_n > max_n) {
      detail::parse_error(
        path, line_no, "vertex count out of range: " + std::to_string(raw_n));
    }
    if (detail::has_leftovers(ls)) {
      detail::parse_error(
        path, line_no, "unexpected text after the vertex count");
    }

    result.n = static_cast<vertex_id>(raw_n);
    found_n = true;
    break;
  }
  if (!found_n) {
    throw std::runtime_error(path.string() +
                             ": empty file, expected the vertex count");
  }

  // 2. edges: one per non-blank line
  while (std::getline(in, line)) {
    ++line_no;
    if (detail::is_skippable(line)) {
      continue;
    }

    std::istringstream ls(line);
    long long raw_u;
    long long raw_v;
    if (!(ls >> raw_u >> raw_v)) {
      detail::parse_error(path, line_no, "expected two vertex ids");
    }
    if (raw_u < 1 || raw_u > result.n || raw_v < 1 || raw_v > result.n) {
      detail::parse_error(
        path, line_no, "vertex out of range 1.." + std::to_string(result.n));
    }
    input_edge_t<W> e{};
    e.u = static_cast<vertex_id>(raw_u);
    e.v = static_cast<vertex_id>(raw_v);

    if constexpr (W::is_weighted) {
      std::string token;
      if (!(ls >> token)) {
        detail::parse_error(path, line_no, "expected an edge weight");
      }
      try {
        e.weight = parse_weight(std::string_view(token));
      } catch (const std::invalid_argument& bad) {
        detail::parse_error(path, line_no, bad.what());
      }
    } else {
      (void)parse_weight;
    }

    if (detail::has_leftovers(ls)) {
      detail::parse_error(path, line_no, "unexpected text after the edge");
    }

    if (e.u == e.v) {
      ++result.skipped_self_loops;
      if constexpr (W::is_weighted) {
        if (e.weight < 0) {
          result.negative_self_loop_edges.push_back(e);
        }
      }
      continue;
    }
    result.edges.push_back(e);
  }

  return result;
}

} // namespace gl::io
