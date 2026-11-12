#pragma once

#include <cstdint>
#include <vector>

#include "cache.hpp"
#include "prefetcher.hpp"

struct PrefetchStats {
  uint64_t issued = 0;         // prefetch fills
  uint64_t useful = 0;         // prefetched lines later hit by a demand access
  uint64_t late = 0;           // useful prefetches that had not arrived yet
  uint64_t demand_misses = 0;  // demand accesses that went to memory

  double accuracy() const { return issued ? double(useful) / issued : 0.0; }
  // Share of would-be memory accesses the prefetcher turned into hits.
  double coverage() const { return useful ? double(useful) / (useful + demand_misses) : 0.0; }
  double lateness() const { return useful ? double(late) / useful : 0.0; }
};

// A chain of caches in front of memory (L1, then L2, ...), modelling a
// blocking core: each access waits for the level that hits, or for memory.
//   cost = L1 latency, plus L2 latency on an L1 miss, plus memory latency on
//   an L2 miss.
// Levels are non-inclusive: a miss fills every level it passed through, and a
// dirty line evicted from level i is written into level i+1 (or memory) off
// the critical path, costing no cycles.
//
// An optional stride prefetcher sits at the last level, so its fills come
// from memory and touch no other level. It trains on that level's demand
// misses and on first demand hits to prefetched lines (misses it hid). A
// prefetch issued at cycle t arrives at t + memory latency; a demand access
// that finds it still in flight waits for the rest and counts as late.
class Hierarchy {
 public:
  struct Level {
    CacheConfig config;
    uint32_t latency;  // cycles to look up this level
  };

  // prefetch_degree 0 turns the prefetcher off.
  Hierarchy(const std::vector<Level> &levels, uint32_t memory_latency,
            uint32_t prefetch_degree = 0);

  // Returns the cycles this access took.
  uint64_t access(uint64_t addr, bool is_write);

  const CacheStats &stats(size_t level) const { return caches_[level].stats(); }
  const Cache &cache(size_t level) const { return caches_[level]; }
  size_t levels() const { return caches_.size(); }
  uint64_t accesses() const { return accesses_; }
  double amat() const { return accesses_ ? double(cycles_) / accesses_ : 0.0; }
  const PrefetchStats &prefetch_stats() const { return prefetch_; }

 private:
  uint64_t last_level(uint64_t addr, const AccessResult &r, uint64_t now);

  std::vector<Cache> caches_;
  std::vector<uint32_t> latencies_;
  uint32_t memory_latency_;
  uint64_t line_bytes_;
  uint64_t accesses_ = 0;
  uint64_t cycles_ = 0;  // also the clock: each access starts when the last one ends
  uint32_t prefetch_degree_;
  StridePrefetcher prefetcher_;
  PrefetchStats prefetch_;
};
