# Conventions

Conventions for the generic graph library (`include/graph/`, namespace `gl`).
Part 1 code (`src/`, `app/`, `include/graph.hpp`) predates these and keeps its
camelCase style until it is migrated.

## Naming

We follow the C++ standard library's style, so the library reads naturally next
to `std::`.

**The rule:** `*_policy` names a concept, `*_type` names a type alias,
"weighting" is the policy's role, and "weight" is the number.

| kind                       | style                  | examples                                   |
| -------------------------- | ---------------------- | ------------------------------------------ |
| types, structs, templates  | `snake_case`           | `undirected`, `weighted<W>`, `csr_storage` |
| concepts                   | `snake_case`           | `direction_policy`, `graph_like`           |
| functions and methods      | `snake_case`           | `vertex_count()`, `for_each_neighbor()`    |
| member type aliases        | `snake_case` + `_type` | `weight_type`, `direction_type`            |
| compile-time boolean flags | `is_` prefix           | `is_directed`, `is_weighted`               |
| template parameters        | `PascalCase`           | `G`, `W`, `F`, `Storage`                   |
| private data members       | trailing underscore    | `offsets_`, `targets_`                     |
| macros                     | `SCREAMING_CASE`       | (avoid where possible)                     |

Never use names starting with `__` or `_` + uppercase letter: they are reserved
for the compiler and standard library.

### Policy axes

| layer                         | direction axis           | weight axis                             |
| ----------------------------- | ------------------------ | --------------------------------------- |
| concept for the axis          | `direction_policy`       | `weight_policy`                         |
| tag types                     | `undirected`, `directed` | `unweighted`, `weighted<W>`             |
| flag on the tag               | `is_directed`            | `is_weighted`                           |
| number type inside the policy | —                        | `weighted<W>::weight_type`              |
| graph alias for the policy    | `G::direction_type`      | `G::weighting_type`                     |
| graph alias for the number    | —                        | `G::weight_type` (weighted graphs only) |

`direction_type` and `weighting_type` are named by the policy's _role_, like
`std::vector::allocator_type`. `weight_type` is the number, like
`std::vector::value_type`. `unweighted` deliberately has no `weight_type`, so
asking for a weight on an unweighted graph fails to compile.

### Name clashes to avoid

- A class member may not share its class's name (`weighted::weighted` is
  ill-formed), hence the `is_` flags.
- A member alias must not share a concept's name, or it hides the concept
  inside the class. Hence `direction_type`, not `direction_policy`.

## Headers and namespaces

- Everything lives in `namespace gl`. Close it with `} // namespace gl`.
- Templates are header-only: their full definitions live in `.hpp` files.
- Every header starts with `#pragma once`.
- Include project headers by their path from `include/`:
  `#include "graph/policies.hpp"`, never relative paths like `../`.
- Include what you use: if a file names something directly, it includes the
  header that declares it, even if another include already pulls it in.
- No `using namespace` in headers. It is acceptable inside a test `.cpp` file.

## Types

- `gl::vertex_id` (`std::uint32_t`) for vertex identifiers and CSR targets.
- `gl::edge_index` (`std::uint64_t`) for CSR offsets.
- Vertices are 1-indexed, matching the input file format and Part 1.
- Edge weights are floating-point (`weighted<W>` requires `std::floating_point W`).

## Errors

- **Programmer errors** (violated preconditions, such as a vertex id out of
  range): `assert`. They abort in debug builds and cost nothing in release.
- **Runtime errors** (missing file, malformed input, negative weights where
  Dijkstra is requested): exceptions, caught at the program's top level.

## Tests

- Framework: doctest, vendored in `third_party/doctest/doctest.h`.
- Run with `make test`. The test build uses `-fsanitize=undefined` and libc++
  hardening (`_LIBCPP_HARDENING_MODE_DEBUG`), which bounds-checks standard
  containers.
- **Compile-time facts** (concepts, type aliases): `static_assert` at file
  scope, one fact per assertion.
- **Runtime behaviour**: `TEST_CASE` blocks with `CHECK` / `REQUIRE`.
- Every concept gets at least one **negative** test: a type that should be
  rejected for exactly one reason (for example `Liar` for `weight_policy`).
  Check that the test fails before the fix and passes after it.
- Mocks used only for concept checks declare their member functions without
  defining them.
- A header is only compiled when a `.cpp` includes it, so every new header gets
  a test file that includes it.
