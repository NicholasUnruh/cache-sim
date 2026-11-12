#pragma once

#include <cstdint>
#include <vector>

// LRU evicts the least recently used line. SRRIP and BRRIP (Jaleel et al.,
// ISCA 2010) keep a 2-bit re-reference prediction value (RRPV) per line and
// evict a line predicted to be re-referenced in the distant future (RRPV 3).
enum class Policy { LRU, SRRIP, BRRIP };

struct CacheConfig {
  uint64_t size_bytes;
  uint32_t ways;
  uint32_t line_bytes;
  Policy policy = Policy::LRU;
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

struct AccessResult {
  bool hit = false;
  bool prefetch_hit = false;    // first demand hit on a prefetched line...
  uint64_t ready_at = 0;        // ...whose data arrives at this cycle
  bool writeback = false;       // the fill evicted a dirty line...
  uint64_t writeback_addr = 0;  // ...whose first byte is here
};

// Set-associative, write-back, write-allocate cache.
// Throws std::invalid_argument unless size, ways and line size are powers of
// two and the cache holds at least one set.
class Cache {
 public:
  explicit Cache(const CacheConfig &config);

  AccessResult access(uint64_t addr, bool is_write);
  // Fills the line holding addr without counting an access, marked as
  // prefetched with data arriving at cycle ready_at. Returns false if the
  // line is already present. A dirty victim counts as a writeback.
  bool prefetch(uint64_t addr, uint64_t ready_at);
  const CacheStats &stats() const { return stats_; }

 private:
  struct Line {
    uint64_t tag = 0;
    uint64_t last_use = 0;  // LRU
    uint8_t rrpv = 0;       // SRRIP and BRRIP
    bool valid = false;
    bool dirty = false;
    bool prefetched = false;  // filled by prefetch, no demand hit yet
    uint64_t ready_at = 0;    // prefetched data arrives at this cycle
  };

  Line *find(uint64_t set, uint64_t tag);
  Line *choose_victim(Line *set);
  uint8_t insertion_rrpv();

  Policy policy_;
  uint32_t ways_;
  uint32_t offset_bits_;
  uint32_t index_bits_;
  std::vector<Line> lines_;  // set-major: set s owns lines_[s * ways_ .. + ways_)
  uint64_t clock_ = 0;
  uint64_t fills_ = 0;
  CacheStats stats_;
};
