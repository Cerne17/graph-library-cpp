CXX := g++
CXXFLAGS := -std=c++20 -Iinclude -Wall -Wextra -g
RELFLAGS := -std=c++20 -Iinclude -Wall -Wextra -O2
TESTFLAGS := -std=c++20 -Iinclude -Ithird_party -Wall -Wextra -g -fsanitize=undefined -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG

OBJS := src/graph.o src/io.o src/search.o src/stats.o src/components.o src/distance.o app/main.o
LIB_SRCS := src/graph.cpp src/io.cpp src/search.cpp src/stats.cpp src/components.cpp src/distance.cpp

TEST_SRCS := $(wildcard tests/test_*.cpp)
TEST_OBJS := $(TEST_SRCS:.cpp=.o)

.PHONY: all release benchmark clean test

all: app/graphs

# Debug build: -g, no optimisation. The default while developing.
app/graphs: $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Optimised build of the same program, for when the code is already debugged
# and you just want the results. Compiled straight from the sources so it can
# never pick up a stale debug object.
release: app/graphs_release

app/graphs_release: app/main.cpp $(LIB_SRCS) include/graph.hpp
	$(CXX) $(RELFLAGS) app/main.cpp $(LIB_SRCS) -o $@

benchmark: app/benchmark

app/benchmark: app/benchmark.cpp $(LIB_SRCS) include/graph.hpp
	$(CXX) $(RELFLAGS) app/benchmark.cpp $(LIB_SRCS) -o $@

HEADERS := $(wildcard include/graph/*.hpp)

tests/%.o: tests/%.cpp $(HEADERS)
	$(CXX) $(TESTFLAGS) -c $< -o $@

%.o: %.cpp include/graph.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TEST_OBJS) app/graphs app/graphs_release app/benchmark tests/run_tests
	rm -rf tests/run_tests.dSYM

tests/run_tests: $(TEST_OBJS)
	$(CXX) $(TESTFLAGS) $^ -o $@

test: tests/run_tests
	./tests/run_tests

