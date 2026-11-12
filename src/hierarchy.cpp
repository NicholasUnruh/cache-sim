#include "hierarchy.hpp"

Hierarchy::Hierarchy(const std::vector<Level> &levels, uint32_t memory_latency, uint32_t prefetch_degree)
    : memory_latency_(memory_latency), prefetch_degree_(prefetch_degree), prefetcher_(prefetch_degree) {
  for (const Level &level : levels) {
    caches_.emplace_back(level.config);
    latencies_.push_back(level.latency);
  }
  line_bytes_ = levels.back().config.line_bytes;
}

uint64_t Hierarchy::access(uint64_t addr, bool is_write) {
  ++accesses_;
  uint64_t cycles = 0;
  for (size_t i = 0; i < caches_.size(); ++i) {
    bool last = i + 1 == caches_.size();
    cycles += latencies_[i];
    // Only L1 sees the core's write; lower levels see a line fill (a read).
    AccessResult r = caches_[i].access(addr, i == 0 && is_write);
    if (r.writeback && !last) caches_[i + 1].access(r.writeback_addr, true);
    if (last) cycles += last_level(addr, r, cycles_ + cycles);
    else if (r.hit) break;
  }
  cycles_ += cycles;
  return cycles;
}

// Handles the last level's result, known at cycle `now`. Returns the cycles
// the access still waits: for memory on a miss, or for a late prefetch.
uint64_t Hierarchy::last_level(uint64_t addr, const AccessResult &r, uint64_t now) {
  uint64_t wait = 0;
  if (!r.hit) {
    ++prefetch_.demand_misses;
    wait = memory_latency_;
  } else if (r.prefetch_hit) {
    ++prefetch_.useful;
    if (r.ready_at > now) {
      ++prefetch_.late;
      wait = r.ready_at - now;
    }
  }
  if (prefetch_degree_ && (!r.hit || r.prefetch_hit))
    for (uint64_t block : prefetcher_.train(addr / line_bytes_))
      if (caches_.back().prefetch(block * line_bytes_, now + memory_latency_)) ++prefetch_.issued;
  return wait;
}
