#include "doctest/doctest.h"
#include "graph/io/label_map.hpp"
#include "graph/types.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
// tests/data/labels_small.txt, by hand
const std::vector<std::string> small_names{ "",
                                            "Edsger W. Dijkstra",
                                            "Alan M. Turing",
                                            "Hopper, Grace",
                                            "Ada Lovelace" };
} // namespace

TEST_CASE("label_map: built from names")
{
  const gl::io::label_map labels({ "", "a", "b", "c" });

  CHECK(labels.size() == 3);
  CHECK(labels.name_of(2) == "b");
  CHECK(labels.id_of("c") == 3);
}

TEST_CASE("label_map: empty map")
{
  const gl::io::label_map labels({ "" });

  CHECK(labels.size() == 0);
  CHECK_THROWS_AS(labels.id_of("a"), std::invalid_argument);
}

TEST_CASE("label_map: duplicate names are rejected")
{
  CHECK_THROWS_WITH_AS(gl::io::label_map({ "", "a", "b", "a" }),
                       "duplicate name: a",
                       std::invalid_argument);
}

TEST_CASE("read_label_map: name_of and id_of agree both ways")
{
  const auto labels = gl::io::read_label_map("tests/data/labels_small.txt");

  REQUIRE(labels.size() == 4);
  for (gl::vertex_id v = 1; v <= labels.size(); ++v) {
    CAPTURE(v);
    CHECK(labels.name_of(v) == small_names[v]);
    CHECK(labels.id_of(small_names[v]) == v);
    CHECK(labels.id_of(labels.name_of(v)) == v);
  }
}

TEST_CASE("read_label_map: a name containing a comma survives intact")
{
  const auto labels = gl::io::read_label_map("tests/data/labels_small.txt");

  CHECK(labels.name_of(3) == "Hopper, Grace");
  CHECK(labels.id_of("Hopper, Grace") == 3);
}

TEST_CASE("read_label_map: lookup with a std::string_view")
{
  const auto labels = gl::io::read_label_map("tests/data/labels_small.txt");
  const std::string text = "say hi to Alan M. Turing!";
  const std::string_view name = std::string_view(text).substr(10, 14);

  CHECK(labels.id_of(name) == 2);
}

TEST_CASE("read_label_map: unknown name throws")
{
  const auto labels = gl::io::read_label_map("tests/data/labels_small.txt");

  CHECK_THROWS_WITH_AS(labels.id_of("Grace Hopper"),
                       "unknown name: Grace Hopper",
                       std::invalid_argument);
  CHECK_THROWS_AS(labels.id_of(""), std::invalid_argument);
}

TEST_CASE("read_label_map: Windows line endings are not part of the name")
{
  const auto labels = gl::io::read_label_map("tests/data/labels_crlf.txt");

  REQUIRE(labels.size() == 2);
  CHECK(labels.name_of(2) == "Alan M. Turing");
  CHECK(labels.id_of("Alan M. Turing") == 2);
}

// ---- errors -----------------------------------------------------------------

TEST_CASE("read_label_map: missing file")
{
  CHECK_THROWS_WITH_AS(
    gl::io::read_label_map("tests/data/does_not_exist.txt"),
    doctest::Contains("could not open file"),
    std::runtime_error);
}

TEST_CASE("read_label_map: line without a comma")
{
  CHECK_THROWS_WITH_AS(
    gl::io::read_label_map("tests/data/labels_no_comma.txt"),
    "tests/data/labels_no_comma.txt:2: expected 'id,name'",
    std::runtime_error);
}

TEST_CASE("read_label_map: id is not a number")
{
  CHECK_THROWS_WITH_AS(
    gl::io::read_label_map("tests/data/labels_bad_id.txt"),
    "tests/data/labels_bad_id.txt:2: vertex id is not a number",
    std::runtime_error);
}

TEST_CASE("read_label_map: id out of order")
{
  CHECK_THROWS_WITH_AS(
    gl::io::read_label_map("tests/data/labels_out_of_order.txt"),
    "tests/data/labels_out_of_order.txt:2: expected vertex id 2, got 3",
    std::runtime_error);
}

TEST_CASE("read_label_map: empty name")
{
  CHECK_THROWS_WITH_AS(
    gl::io::read_label_map("tests/data/labels_empty_name.txt"),
    "tests/data/labels_empty_name.txt:2: empty name",
    std::runtime_error);
}

TEST_CASE("read_label_map: duplicate name")
{
  CHECK_THROWS_WITH_AS(
    gl::io::read_label_map("tests/data/labels_duplicate.txt"),
    "duplicate name: Edsger W. Dijkstra",
    std::invalid_argument);
}
