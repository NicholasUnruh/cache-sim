#include "cache.hpp"

#include <stdexcept>

static bool is_pow2(uint64_t x) { return x != 0 && (x & (x - 1)) == 0; }

static uint32_t log2u(uint64_t x) {
  uint32_t n = 0;
  while (x >>= 1) ++n;
  return n;
}

Cache::Cache(const CacheConfig &config) : ways_(config.ways) {
  if (!is_pow2(config.size_bytes) || !is_pow2(config.ways) ||
      !is_pow2(config.line_bytes) ||
      config.size_bytes < uint64_t(config.ways) * config.line_bytes)
    throw std::invalid_argument("size, ways and line must be powers of two, size >= ways * line");
  uint64_t sets = config.size_bytes / (uint64_t(config.ways) * config.line_bytes);
  offset_bits_ = log2u(config.line_bytes);
  index_bits_ = log2u(sets);
  lines_.resize(sets * config.ways);
}

bool Cache::access(uint64_t addr, bool is_write) {
  ++clock_;
  is_write ? ++stats_.writes : ++stats_.reads;
  uint64_t block = addr >> offset_bits_;
  uint64_t set = block & ((uint64_t(1) << index_bits_) - 1);
  uint64_t tag = block >> index_bits_;
  Line *first = &lines_[set * ways_];

  Line *victim = first;
  for (Line *line = first; line != first + ways_; ++line) {
    if (line->valid && line->tag == tag) {
      line->last_use = clock_;
      line->dirty |= is_write;
      return true;
    }
    // Prefer an invalid way, then the least recently used one.
    if (victim->valid && (!line->valid || line->last_use < victim->last_use)) victim = line;
  }

  is_write ? ++stats_.write_misses : ++stats_.read_misses;
  if (victim->valid && victim->dirty) ++stats_.writebacks;
  *victim = Line{tag, clock_, true, is_write};
  return false;
}
