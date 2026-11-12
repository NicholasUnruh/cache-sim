#include "hierarchy.hpp"

Hierarchy::Hierarchy(const std::vector<Level> &levels, uint32_t memory_latency)
    : memory_latency_(memory_latency) {
  for (const Level &level : levels) {
    caches_.emplace_back(level.config);
    latencies_.push_back(level.latency);
  }
}

uint64_t Hierarchy::access(uint64_t addr, bool is_write) {
  ++accesses_;
  uint64_t cycles = 0;
  bool hit = false;
  for (size_t i = 0; i < caches_.size() && !hit; ++i) {
    cycles += latencies_[i];
    // Only L1 sees the core's write; lower levels see a line fill (a read).
    AccessResult r = caches_[i].access(addr, i == 0 && is_write);
    if (r.writeback && i + 1 < caches_.size()) caches_[i + 1].access(r.writeback_addr, true);
    hit = r.hit;
  }
  if (!hit) cycles += memory_latency_;
  cycles_ += cycles;
  return cycles;
}
