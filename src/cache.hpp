#pragma once

#include <cstdint>
#include <vector>

struct CacheConfig {
  uint64_t size_bytes;
  uint32_t ways;
  uint32_t line_bytes;
};

struct CacheStats {
  uint64_t reads = 0;
  uint64_t writes = 0;
  uint64_t read_misses = 0;
  uint64_t write_misses = 0;
  uint64_t writebacks = 0;

  uint64_t accesses() const { return reads + writes; }
  uint64_t misses() const { return read_misses + write_misses; }
};

// Set-associative, write-back, write-allocate cache with true LRU replacement.
// Throws std::invalid_argument unless size, ways and line size are powers of
// two and the cache holds at least one set.
class Cache {
 public:
  explicit Cache(const CacheConfig &config);

  // Returns true on a hit.
  bool access(uint64_t addr, bool is_write);
  const CacheStats &stats() const { return stats_; }

 private:
  struct Line {
    uint64_t tag = 0;
    uint64_t last_use = 0;
    bool valid = false;
    bool dirty = false;
  };

  uint32_t ways_;
  uint32_t offset_bits_;
  uint32_t index_bits_;
  std::vector<Line> lines_;  // set-major: set s owns lines_[s * ways_ .. + ways_)
  uint64_t clock_ = 0;
  CacheStats stats_;
};
