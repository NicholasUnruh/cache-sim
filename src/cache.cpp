#include "cache.hpp"

#include <algorithm>
#include <stdexcept>

static constexpr uint8_t kMaxRrpv = 3;      // 2-bit RRPV: 0 = near, 3 = distant re-reference
static constexpr uint64_t kBrripLong = 32;  // BRRIP inserts every 32nd fill at "long", not "distant"

static bool is_pow2(uint64_t x) { return x != 0 && (x & (x - 1)) == 0; }

static uint32_t log2u(uint64_t x) {
  uint32_t n = 0;
  while (x >>= 1) ++n;
  return n;
}

Cache::Cache(const CacheConfig &config) : policy_(config.policy), ways_(config.ways) {
  if (!is_pow2(config.size_bytes) || !is_pow2(config.ways) ||
      !is_pow2(config.line_bytes) ||
      config.size_bytes < uint64_t(config.ways) * config.line_bytes)
    throw std::invalid_argument("size, ways and line must be powers of two, size >= ways * line");
  uint64_t sets = config.size_bytes / (uint64_t(config.ways) * config.line_bytes);
  offset_bits_ = log2u(config.line_bytes);
  index_bits_ = log2u(sets);
  lines_.resize(sets * config.ways);
}

AccessResult Cache::access(uint64_t addr, bool is_write) {
  ++clock_;
  is_write ? ++stats_.writes : ++stats_.reads;
  uint64_t block = addr >> offset_bits_;
  uint64_t set = block & ((uint64_t(1) << index_bits_) - 1);
  uint64_t tag = block >> index_bits_;

  AccessResult result;
  if (Line *line = find(set, tag)) {
    line->last_use = clock_;
    line->rrpv = 0;  // hit priority: predict a near re-reference
    line->dirty |= is_write;
    result.hit = true;
    result.prefetch_hit = line->prefetched;
    result.ready_at = line->ready_at;
    line->prefetched = false;
    return result;
  }

  is_write ? ++stats_.write_misses : ++stats_.read_misses;
  Line *victim = choose_victim(&lines_[set * ways_]);
  if (victim->valid && victim->dirty) {
    ++stats_.writebacks;
    result.writeback = true;
    result.writeback_addr = ((victim->tag << index_bits_) | set) << offset_bits_;
  }
  *victim = Line{tag, clock_, insertion_rrpv(), true, is_write};
  return result;
}

bool Cache::prefetch(uint64_t addr, uint64_t ready_at) {
  ++clock_;
  uint64_t block = addr >> offset_bits_;
  uint64_t set = block & ((uint64_t(1) << index_bits_) - 1);
  uint64_t tag = block >> index_bits_;
  if (find(set, tag)) return false;
  Line *victim = choose_victim(&lines_[set * ways_]);
  if (victim->valid && victim->dirty) ++stats_.writebacks;  // to memory: prefetching is last-level only
  *victim = Line{tag, clock_, insertion_rrpv(), true, false, true, ready_at};
  return true;
}

Cache::Line *Cache::find(uint64_t set, uint64_t tag) {
  Line *first = &lines_[set * ways_];
  for (Line *line = first; line != first + ways_; ++line)
    if (line->valid && line->tag == tag) return line;
  return nullptr;
}

Cache::Line *Cache::choose_victim(Line *set) {
  Line *end = set + ways_;
  Line *invalid = std::find_if(set, end, [](const Line &l) { return !l.valid; });
  if (invalid != end) return invalid;
  if (policy_ == Policy::LRU)
    return std::min_element(set, end, [](const Line &a, const Line &b) { return a.last_use < b.last_use; });

  // RRIP: evict the first line with RRPV 3. When none has it, the hardware
  // ages every line by one and looks again; adding the oldest line's distance
  // to 3 does all those rounds at once.
  Line *victim = std::max_element(set, end, [](const Line &a, const Line &b) { return a.rrpv < b.rrpv; });
  uint8_t age = kMaxRrpv - victim->rrpv;
  for (Line *line = set; line != end; ++line) line->rrpv += age;
  return victim;
}

uint8_t Cache::insertion_rrpv() {
  ++fills_;
  // SRRIP predicts a "long" re-reference (RRPV 2), so a line must be hit once
  // to outlive a scan. BRRIP predicts "distant" (RRPV 3) for all but every
  // 32nd fill, so a working set larger than the cache keeps most of what it
  // already holds instead of thrashing.
  if (policy_ == Policy::SRRIP || (policy_ == Policy::BRRIP && fills_ % kBrripLong == 0))
    return kMaxRrpv - 1;
  return kMaxRrpv;
}
