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

.PHONY: all release benchmark clean test

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
