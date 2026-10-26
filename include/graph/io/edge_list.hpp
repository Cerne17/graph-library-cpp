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
#include <vector>

namespace gl {

template<weight_policy W>
struct edge_list
{
  vertex_id n = 0;
  std::vector<input_edge_t<W>> edges;
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

} // namespace detail

// Reads "n" on the first non-blank line, then one edge per line:
// "u v" for unweighted policies, "u v w" for weighted ones.
// Blank lines and comments (marked with a starting '#') are skipped. Any
// malformed input throws std::runtime_error.
template<weight_policy W>
edge_list<W> read_edge_list(const std::filesystem::path& path)
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
    if (raw_u == raw_v) {
      detail::parse_error(
        path, line_no, "self-loop on vertex " + std::to_string(raw_u));
    }

    input_edge_t<W> e{};
    e.u = static_cast<vertex_id>(raw_u);
    e.v = static_cast<vertex_id>(raw_v);

    if constexpr (W::is_weighted) {
      if (!(ls >> e.weight)) {
        detail::parse_error(path, line_no, "expected an edge weight");
      }
    }

    if (detail::has_leftovers(ls)) {
      detail::parse_error(path, line_no, "unexpected text after the edge");
    }

    result.edges.push_back(e);
  }

  return result;
}

} // namespace gl
