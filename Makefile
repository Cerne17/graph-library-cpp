CXX := g++
CXXFLAGS := -std=c++23 -Iinclude -Wall -Wextra -g
RELFLAGS := -std=c++23 -Iinclude -Wall -Wextra -O2
# Tests bounds-check the standard containers with whichever standard library
# is in use: libc++ hardening (macOS/clang) or libstdc++ assertions (Linux/GCC).
# Each library ignores the other's macro.
TESTFLAGS := -std=c++23 -Iinclude -Ithird_party -Wall -Wextra -g -fsanitize=undefined -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG -D_GLIBCXX_ASSERTIONS

# AddressSanitizer on Linux only: with this toolchain it hangs at startup on macOS.
ifeq ($(shell uname),Linux)
TESTFLAGS += -fsanitize=address
endif

# Every generated file lives under build/, mirroring the source tree:
# src/graph.cpp -> build/src/graph.o, tests/test_csr.cpp -> build/tests/test_csr.o
BUILD := build

LIB_SRCS := src/graph.cpp src/io.cpp src/search.cpp src/stats.cpp src/components.cpp src/distance.cpp
OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(LIB_SRCS) app/main.cpp)

TEST_SRCS := $(wildcard tests/test_*.cpp)
TEST_OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(TEST_SRCS))

# Part 1 sources rebuilt with the test flags, so tests can compare the new
# algorithms against the old ones: src/graph.cpp -> build/tests/legacy/src/graph.o
LEGACY_TEST_OBJS := $(patsubst %.cpp,$(BUILD)/tests/legacy/%.o,$(LIB_SRCS))

.PHONY: all release benchmark benchmark-weighted benchmark-part3 clean test

all: $(BUILD)/graphs

# Debug build: -g, no optimisation. The default while developing.
$(BUILD)/graphs: $(OBJS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Optimised build of the same program, for when the code is already debugged
# and you just want the results. Compiled straight from the sources so it can
# never pick up a stale debug object.
release: $(BUILD)/graphs_release

$(BUILD)/graphs_release: app/main.cpp $(LIB_SRCS) include/graph.hpp
	@mkdir -p $(@D)
	$(CXX) $(RELFLAGS) app/main.cpp $(LIB_SRCS) -o $@

benchmark: $(BUILD)/benchmark

$(BUILD)/benchmark: app/benchmark.cpp $(LIB_SRCS) include/graph.hpp
	@mkdir -p $(@D)
	$(CXX) $(RELFLAGS) app/benchmark.cpp $(LIB_SRCS) -o $@

HEADERS := $(wildcard include/graph/*.hpp) $(wildcard include/graph/*/*.hpp)

# Part 2 benchmark on the new header-only library. -DNDEBUG: the library's
# precondition asserts cost nothing in a measured build.
benchmark-weighted: $(BUILD)/benchmark_weighted

$(BUILD)/benchmark_weighted: app/benchmark_weighted.cpp $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(RELFLAGS) -DNDEBUG $< -o $@

# Part 3 benchmark (directed graphs, Bellman-Ford, Floyd-Warshall), driven by
# analysis/harness_part3.py. The checked build runs under the test flags for
# the program tests (make test-part3).
benchmark-part3: $(BUILD)/benchmark_part3

$(BUILD)/benchmark_part3: app/benchmark_part3.cpp $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(RELFLAGS) -DNDEBUG $< -o $@

$(BUILD)/benchmark_part3_checked: app/benchmark_part3.cpp $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(TESTFLAGS) $< -o $@

.PHONY: test-part3
test-part3: $(BUILD)/benchmark_part3_checked
	PART3_BINARY=$(abspath $(BUILD)/benchmark_part3_checked) $(PYTHON) -m unittest discover -s tests -p 'test_harness_part3.py' -v

$(BUILD)/tests/test_main.o: tests/test_main.cpp # prevents doctests recompiling
	@mkdir -p $(@D)
	$(CXX) $(TESTFLAGS) -c $< -o $@

$(BUILD)/tests/legacy/src/%.o: src/%.cpp include/graph.hpp
	@mkdir -p $(@D)
	$(CXX) $(TESTFLAGS) -c $< -o $@

$(BUILD)/tests/%.o: tests/%.cpp $(HEADERS) include/graph.hpp # graph.hpp: legacy comparison test
	@mkdir -p $(@D)
	$(CXX) $(TESTFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp include/graph.hpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

$(BUILD)/run_tests: $(TEST_OBJS) $(LEGACY_TEST_OBJS)
	@mkdir -p $(@D)
	$(CXX) $(TESTFLAGS) $^ -o $@

test: $(BUILD)/run_tests
	./$(BUILD)/run_tests

# Unified dataset collector; existing Part 1 and Part 2 binaries are unchanged.
.PHONY: collector test-collector
collector: $(BUILD)/collect_stats

$(BUILD)/collect_stats: app/collect_stats.cpp $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(RELFLAGS) -DNDEBUG $< -o $@

$(BUILD)/collect_stats_checked: app/collect_stats.cpp $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(TESTFLAGS) $< -o $@

test-collector: collector $(BUILD)/collect_stats_checked
	COLLECTOR_BINARY=$(abspath $(BUILD)/collect_stats_checked) python3 -m unittest discover -s tests -p 'test_collector.py' -v

# Saved figures only: no graph algorithms or timing runs.
PYTHON ?= python3
.PHONY: figures test-visualizations
figures:
	$(PYTHON) analysis/plot_graphs.py

test-visualizations:
	$(PYTHON) -m unittest discover -s tests -p 'test_visualization*.py' -v

.PHONY: baselines
baselines: $(BUILD)/baseline_cases
	$(PYTHON) analysis/run_baselines.py

$(BUILD)/baseline_cases: app/baseline_cases.cpp $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(RELFLAGS) -DNDEBUG $< -o $@

.PHONY: test-traversals
test-traversals:
	$(PYTHON) -m unittest discover -s tests -p 'test_traversals.py' -v
