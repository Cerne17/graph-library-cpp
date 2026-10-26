#include "doctest/doctest.h"
#include "graph/io/edge_list.hpp"
#include "graph/policies.hpp"
#include "graph/types.hpp"

#include <concepts>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

static_assert(
  std::same_as<decltype(gl::edge_list<gl::unweighted>::edges)::value_type,
               gl::edge>);
static_assert(
  std::same_as<decltype(gl::edge_list<gl::weighted<>>::edges)::value_type,
               gl::weighted_edge<double>>);
static_assert(
  std::same_as<decltype(gl::edge_list<gl::weighted<float>>::edges)::value_type,
               gl::weighted_edge<float>>);

namespace {
// Writes `content` byte for byte to a fresh file in the system temp directory
// and removes it when it goes out of scope. Binary mode keeps "\r\n" intact.
struct temp_file
{
  std::filesystem::path path;

  explicit temp_file(const std::string& content)
  {
    static int counter = 0;
    path = std::filesystem::temp_directory_path() /
           ("gl_test_edge_list_" + std::to_string(counter++) + ".txt");
    std::ofstream out(path, std::ios::binary);
    out << content;
  }

  ~temp_file() { std::filesystem::remove(path); }

  temp_file(const temp_file&) = delete;
  temp_file& operator=(const temp_file&) = delete;
};

template<gl::weight_policy W>
gl::edge_list<W> read_string(const std::string& content)
{
  const temp_file f(content);
  return gl::read_edge_list<W>(f.path);
}
} // namespace

// ---- well-formed input ----------------------------------------------------

TEST_CASE("read_edge_list: unweighted file")
{
  const auto g =
    gl::read_edge_list<gl::unweighted>("tests/data/unweighted_small.txt");

  CHECK(g.n == 3);
  REQUIRE(g.edges.size() == 2);
  CHECK(g.edges[0].u == 1);
  CHECK(g.edges[0].v == 2);
  CHECK(g.edges[1].u == 2);
  CHECK(g.edges[1].v == 3);
}

TEST_CASE("read_edge_list: weighted file")
{
  const auto g =
    gl::read_edge_list<gl::weighted<>>("tests/data/weighted_small.txt");

  CHECK(g.n == 3);
  REQUIRE(g.edges.size() == 2);
  CHECK(g.edges[0].u == 1);
  CHECK(g.edges[0].v == 2);
  CHECK(g.edges[0].weight == 0.5);
  CHECK(g.edges[1].u == 2);
  CHECK(g.edges[1].v == 3);
  CHECK(g.edges[1].weight == 1.5);
}

TEST_CASE("read_edge_list: float weights")
{
  const auto g =
    gl::read_edge_list<gl::weighted<float>>("tests/data/weighted_small.txt");

  REQUIRE(g.edges.size() == 2);
  CHECK(g.edges[0].weight == 0.5f);
  CHECK(g.edges[1].weight == 1.5f);
}

TEST_CASE("read_edge_list: edges keep file order and orientation")
{
  const auto g = read_string<gl::unweighted>("4\n3 1\n4 2\n1 4\n");

  REQUIRE(g.edges.size() == 3);
  CHECK(g.edges[0].u == 3);
  CHECK(g.edges[0].v == 1);
  CHECK(g.edges[1].u == 4);
  CHECK(g.edges[1].v == 2);
  CHECK(g.edges[2].u == 1);
  CHECK(g.edges[2].v == 4);
}

TEST_CASE("read_edge_list: vertex count without edges")
{
  const auto g = read_string<gl::unweighted>("5\n");

  CHECK(g.n == 5);
  CHECK(g.edges.empty());
}

TEST_CASE("read_edge_list: zero vertices")
{
  const auto g = read_string<gl::unweighted>("0\n");

  CHECK(g.n == 0);
  CHECK(g.edges.empty());
}

TEST_CASE("read_edge_list: largest accepted vertex count")
{
  // vertex_id max - 2, so csr_storage's n + 2 offsets still fit
  const auto g = read_string<gl::unweighted>("4294967293\n");

  CHECK(g.n == 4294967293u);
}

TEST_CASE("read_edge_list: last line without a trailing newline")
{
  const auto g = read_string<gl::weighted<>>("3\n1 2 0.5\n2 3 1.5");

  REQUIRE(g.edges.size() == 2);
  CHECK(g.edges[1].weight == 1.5);
}

TEST_CASE("read_edge_list: blank and whitespace-only lines are skipped")
{
  const auto g =
    read_string<gl::unweighted>("\n   \n3\n\n\t\n1 2\n  \n2 3\n\n");

  CHECK(g.n == 3);
  CHECK(g.edges.size() == 2);
}

TEST_CASE("read_edge_list: Windows line endings")
{
  const auto g = read_string<gl::weighted<>>("3\r\n1 2 0.5\r\n\r\n2 3 1.5\r\n");

  CHECK(g.n == 3);
  REQUIRE(g.edges.size() == 2);
  CHECK(g.edges[1].weight == 1.5);
}

TEST_CASE("read_edge_list: tabs and extra spaces between fields")
{
  const auto g = read_string<gl::weighted<>>("  3  \n1\t2\t0.5\n  2   3   1.5  \n");

  CHECK(g.n == 3);
  REQUIRE(g.edges.size() == 2);
  CHECK(g.edges[0].weight == 0.5);
  CHECK(g.edges[1].u == 2);
  CHECK(g.edges[1].v == 3);
}

TEST_CASE("read_edge_list: comment lines are skipped")
{
  const auto g = read_string<gl::unweighted>(
    "# header comment\n3\n# between edges\n1 2\n   # indented\n2 3\n");

  CHECK(g.n == 3);
  CHECK(g.edges.size() == 2);
}

TEST_CASE("read_edge_list: weight formats")
{
  const auto g = read_string<gl::weighted<>>(
    "4\n1 2 -0.5\n2 3 1e-3\n3 4 7\n"); // negative, scientific, integer

  REQUIRE(g.edges.size() == 3);
  CHECK(g.edges[0].weight == -0.5); // rejecting negatives is not the reader's job
  CHECK(g.edges[1].weight == doctest::Approx(0.001));
  CHECK(g.edges[2].weight == 7.0);
}

// ---- file-level errors ----------------------------------------------------

TEST_CASE("read_edge_list: missing file")
{
  CHECK_THROWS_WITH_AS(
    gl::read_edge_list<gl::unweighted>("tests/data/does_not_exist.txt"),
    doctest::Contains("could not open file"),
    std::runtime_error);
}

TEST_CASE("read_edge_list: empty file")
{
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>(""),
                       doctest::Contains("empty file"),
                       std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("\n  \n\t\n"),
                       doctest::Contains("empty file"),
                       std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("# only a comment\n"),
                       doctest::Contains("empty file"),
                       std::runtime_error);
}

// ---- vertex count errors --------------------------------------------------

TEST_CASE("read_edge_list: vertex count is not a number")
{
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("abc\n"),
                       doctest::Contains("expected the vertex count"),
                       std::runtime_error);
}

TEST_CASE("read_edge_list: vertex count out of range")
{
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("-1\n"),
                       doctest::Contains("vertex count out of range"),
                       std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("4294967294\n"),
                       doctest::Contains("vertex count out of range"),
                       std::runtime_error);
}

TEST_CASE("read_edge_list: text after the vertex count")
{
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("3 4\n1 2\n"),
                       doctest::Contains("unexpected text after the vertex count"),
                       std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("3.5\n"),
                       doctest::Contains("unexpected text after the vertex count"),
                       std::runtime_error);
}

// ---- edge errors ----------------------------------------------------------

TEST_CASE("read_edge_list: edge with fewer than two vertex ids")
{
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("3\n1\n"),
                       doctest::Contains("expected two vertex ids"),
                       std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("3\n1 x\n"),
                       doctest::Contains("expected two vertex ids"),
                       std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("3\n99999999999999999999 1\n"),
                       doctest::Contains("expected two vertex ids"),
                       std::runtime_error); // overflows long long
}

TEST_CASE("read_edge_list: vertex id out of range")
{
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("3\n0 1\n"),
                       doctest::Contains("vertex out of range 1..3"),
                       std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("3\n1 4\n"),
                       doctest::Contains("vertex out of range 1..3"),
                       std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("3\n-1 2\n"),
                       doctest::Contains("vertex out of range 1..3"),
                       std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("0\n1 2\n"),
                       doctest::Contains("vertex out of range"),
                       std::runtime_error); // no vertex is valid when n = 0
}

TEST_CASE("read_edge_list: self-loops are rejected")
{
  CHECK_THROWS_AS(
    gl::read_edge_list<gl::weighted<>>("tests/data/bad_self_loop.txt"),
    std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("3\n2 2\n"),
                       doctest::Contains("self-loop on vertex 2"),
                       std::runtime_error);
}

TEST_CASE("read_edge_list: weighted edge without a valid weight")
{
  CHECK_THROWS_WITH_AS(
    gl::read_edge_list<gl::weighted<>>("tests/data/unweighted_small.txt"),
    doctest::Contains("expected an edge weight"),
    std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::weighted<>>("3\n1 2 abc\n"),
                       doctest::Contains("expected an edge weight"),
                       std::runtime_error);
}

TEST_CASE("read_edge_list: text after the edge")
{
  // a weighted file read as unweighted leaves the weight over
  CHECK_THROWS_WITH_AS(
    gl::read_edge_list<gl::unweighted>("tests/data/weighted_small.txt"),
    doctest::Contains("unexpected text after the edge"),
    std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::weighted<>>("3\n1 2 0.5 x\n"),
                       doctest::Contains("unexpected text after the edge"),
                       std::runtime_error);
  CHECK_THROWS_WITH_AS(read_string<gl::unweighted>("3\n1 2.5\n"),
                       doctest::Contains("unexpected text after the edge"),
                       std::runtime_error);
}

TEST_CASE("read_edge_list: errors name the file and the line")
{
  const temp_file f("3\n1 2\n\n# comment\n1 4\n"); // bad edge on line 5
  const std::string expected = f.path.string() + ":5: vertex out of range 1..3";

  CHECK_THROWS_WITH_AS(gl::read_edge_list<gl::unweighted>(f.path),
                       expected.c_str(),
                       std::runtime_error);
}
