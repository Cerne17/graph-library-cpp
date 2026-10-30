#pragma once

#include "graph/io/edge_list.hpp"
#include "graph/types.hpp"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gl::io {
namespace detail {
// Transparent hash: lets the map be searched with a std::string_view without
// building a temporary std::string.
struct string_hash
{
  using is_transparent = void;

  std::size_t operator()(std::string_view s) const noexcept
  {
    return std::hash<std::string_view>{}(s);
  }
};
} // namespace detail

// Two-way mapping between vertex ids (1..n) and their names.
class label_map
{
private:
  std::vector<std::string> names_; // names_[v], index 0 unused
  std::unordered_map<std::string, vertex_id, detail::string_hash, std::equal_to<>>
    ids_;
  std::size_t duplicates_ = 0; // names that repeat an earlier vertex's name

public:
  // names[v] is the name of vertex v; names[0] is a placeholder.
  // When several vertices share a name, id_of returns the smallest of their
  // ids (the first one in the file); name_of still works for every id.
  explicit label_map(std::vector<std::string> names);

  vertex_id size() const noexcept
  {
    return static_cast<vertex_id>(names_.size() - 1);
  }

  const std::string& name_of(vertex_id v) const
  {
    assert(v >= 1 && v <= size() && "name_of: v out of range");
    return names_[v];
  }

  // The smallest id with this name. Throws std::invalid_argument when no
  // vertex has it.
  vertex_id id_of(std::string_view name) const;

  // How many vertices carry a name an earlier vertex already has, so that
  // id_of cannot reach them by name (n - number of distinct names).
  std::size_t duplicate_count() const noexcept { return duplicates_; }
};

inline label_map::label_map(std::vector<std::string> names)
  : names_(std::move(names))
{
  assert(!names_.empty() && "label_map: names[0] placeholder missing");
  ids_.reserve(names_.size() - 1);
  for (vertex_id v = 1; v < names_.size(); ++v) {
    if (!ids_.emplace(names_[v], v).second) {
      ++duplicates_; // the first id keeps the name
    }
  }
}

inline vertex_id label_map::id_of(std::string_view name) const
{
  const auto it = ids_.find(name); // compiles only because ids_ is transparent
  if (it == ids_.end()) {
    throw std::invalid_argument("unknown name: " + std::string(name));
  }
  return it->second;
}

// Reads "id,name" lines with ids 1, 2, 3... in order. The line is split at
// its first comma, so names may contain commas. Blank lines and '#' comments
// are skipped. Malformed input throws std::runtime_error ("path:line: ...").
// Repeated names are allowed: see label_map::id_of.
inline label_map read_label_map(const std::filesystem::path& path)
{
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("could not open file " + path.string());
  }

  std::vector<std::string> names{ "" }; // placeholder for index 0
  std::string line;
  std::size_t line_no = 0;

  while (std::getline(in, line)) {
    ++line_no;
    if (detail::is_skippable(line)) {
      continue;
    }
    if (!line.empty() && line.back() == '\r') {
      line.pop_back(); // Windows line ending: not part of the name
    }

    const auto comma = line.find(',');
    if (comma == std::string::npos) {
      detail::parse_error(path, line_no, "expected 'id,name'");
    }

    std::istringstream id_field(line.substr(0, comma));
    long long raw_id;
    if (!(id_field >> raw_id) || detail::has_leftovers(id_field)) {
      detail::parse_error(path, line_no, "vertex id is not a number");
    }
    const auto expected = static_cast<long long>(names.size());
    if (raw_id != expected) {
      detail::parse_error(path,
                              line_no,
                              "expected vertex id " + std::to_string(expected) +
                                ", got " + std::to_string(raw_id));
    }

    std::string name = line.substr(comma + 1);
    if (name.empty()) {
      detail::parse_error(path, line_no, "empty name");
    }
    names.push_back(std::move(name));
  }

  return label_map(std::move(names));
}
} // namespace gl::io
