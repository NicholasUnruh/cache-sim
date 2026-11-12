.PHONY: check test smoke clean

CXX ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Werror
LIB := src/cache.cpp src/cache.hpp src/hierarchy.cpp src/hierarchy.hpp src/prefetcher.hpp

# Run before every commit.
check: test smoke

build/cachesim: src/main.cpp $(LIB)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp src/cache.cpp src/hierarchy.cpp

build/test_cache: tests/test_cache.cpp $(LIB)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ tests/test_cache.cpp src/cache.cpp src/hierarchy.cpp

test: build/test_cache
	./build/test_cache

# Two sequential passes over twice the cache size: LRU misses every access.
smoke: build/cachesim
	python3 tools/gen_trace.py stride --bytes 65536 --passes 2 > build/smoke.trace
	./build/cachesim --size 32K --ways 8 --line 64 build/smoke.trace | tee build/smoke.txt
	grep -qE '^L1 .* 100.00%' build/smoke.txt

clean:
	rm -rf build
