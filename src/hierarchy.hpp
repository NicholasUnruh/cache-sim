#pragma once

#include <cstdint>
#include <vector>

#include "cache.hpp"

// A chain of caches in front of memory (L1, then L2, ...), modelling a
// blocking core: each access waits for the level that hits, or for memory.
//   cost = L1 latency, plus L2 latency on an L1 miss, plus memory latency on
//   an L2 miss.
// Levels are non-inclusive: a miss fills every level it passed through, and a
// dirty line evicted from level i is written into level i+1 (or memory) off
// the critical path, costing no cycles.
class Hierarchy {
 public:
  struct Level {
    CacheConfig config;
    uint32_t latency;  // cycles to look up this level
  };

  Hierarchy(const std::vector<Level> &levels, uint32_t memory_latency);

  // Returns the cycles this access took.
  uint64_t access(uint64_t addr, bool is_write);

  const CacheStats &stats(size_t level) const { return caches_[level].stats(); }
  size_t levels() const { return caches_.size(); }
  uint64_t accesses() const { return accesses_; }
  double amat() const { return accesses_ ? double(cycles_) / accesses_ : 0.0; }

 private:
  std::vector<Cache> caches_;
  std::vector<uint32_t> latencies_;
  uint32_t memory_latency_;
  uint64_t accesses_ = 0;
  uint64_t cycles_ = 0;
};
