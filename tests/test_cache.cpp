#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "../src/cache.hpp"
#include "../src/hierarchy.hpp"

static int failures = 0;
#define EXPECT(cond)                                                    \
  do {                                                                  \
    if (!(cond)) {                                                      \
      fprintf(stderr, "%s:%d: FAILED %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                       \
    }                                                                   \
  } while (0)

static void same_line_hits() {
  Cache c({1024, 2, 64});
  EXPECT(!c.access(0x100, false).hit);
  EXPECT(c.access(0x100, false).hit);
  EXPECT(c.access(0x13f, false).hit);  // last byte of the same line
  EXPECT(!c.access(0x140, false).hit);
}

static void direct_mapped_conflict() {
  Cache c({256, 1, 64});  // 4 sets: 0x000 and 0x100 share set 0
  EXPECT(!c.access(0x000, false).hit);
  EXPECT(!c.access(0x100, false).hit);
  EXPECT(!c.access(0x000, false).hit);
  EXPECT(c.stats().misses() == 3);
}

static void lru_eviction() {
  Cache c({128, 2, 64});                // one set, two ways
  EXPECT(!c.access(0x000, false).hit);  // A
  EXPECT(!c.access(0x040, false).hit);  // B
  EXPECT(c.access(0x000, false).hit);   // A is now most recent
  EXPECT(!c.access(0x080, false).hit);  // C evicts B
  EXPECT(c.access(0x000, false).hit);
  EXPECT(!c.access(0x040, false).hit);
}

// 4 sets, two ways: 0x000, 0x100, 0x200 all map to set 0 with tags 0, 1, 2.
static void ways_of_reports_set_contents() {
  Cache c({512, 2, 64});
  EXPECT(c.set_of(0x240) == 1 && c.tag_of(0x240) == 2);
  EXPECT((c.ways_of(0x000) == std::vector<int64_t>{-1, -1}));
  c.access(0x000, false);
  c.access(0x100, false);
  c.access(0x200, false);  // evicts tag 0 from way 0
  EXPECT((c.ways_of(0x000) == std::vector<int64_t>{2, 1}));
}

static void dirty_writeback() {
  Cache c({64, 1, 64});  // one line
  EXPECT(!c.access(0x000, true).hit);
  EXPECT(!c.access(0x040, false).hit);  // evicts the dirty line
  EXPECT(!c.access(0x000, false).hit);  // evicts a clean line
  EXPECT(c.stats().writebacks == 1);
  EXPECT(c.stats().writes == 1 && c.stats().write_misses == 1);
  EXPECT(c.stats().reads == 2 && c.stats().read_misses == 2);
}

// One set, two ways; A B A C D A. SRRIP inserts at RRPV 2 and a hit sets 0:
//   A,B fill (2,2); A hits (0,2); C ages B to 3 and replaces it (A 1, C 2);
//   D ages both by one and replaces C (A 2, D 2); A hits.
// LRU instead evicts A on D, because C was used more recently.
static void srrip_keeps_reused_line_through_scan() {
  const uint64_t trace[] = {0x000, 0x040, 0x000, 0x080, 0x0c0, 0x000};
  Cache srrip({128, 2, 64, Policy::SRRIP});
  Cache lru({128, 2, 64, Policy::LRU});
  for (uint64_t addr : trace) {
    srrip.access(addr, false);
    lru.access(addr, false);
  }
  EXPECT(srrip.stats().misses() == 4);  // only the final A differs
  EXPECT(lru.stats().misses() == 5);
}

// One set, two ways; A B C repeated 10 times. LRU and SRRIP evict each line
// just before its reuse: 30 misses. BRRIP inserts at RRPV 3, so each miss
// replaces way 0 and B, parked in way 1, hits in rounds 2-10. The run makes
// 21 fills, fewer than the 32 needed for BRRIP's first RRPV-2 insertion.
static void brrip_resists_thrashing() {
  const uint64_t abc[] = {0x000, 0x040, 0x080};
  Cache lru({128, 2, 64, Policy::LRU});
  Cache srrip({128, 2, 64, Policy::SRRIP});
  Cache brrip({128, 2, 64, Policy::BRRIP});
  for (int round = 0; round < 10; ++round)
    for (uint64_t addr : abc) {
      lru.access(addr, false);
      srrip.access(addr, false);
      brrip.access(addr, false);
    }
  EXPECT(lru.stats().misses() == 30);
  EXPECT(srrip.stats().misses() == 30);
  EXPECT(brrip.stats().misses() == 21);
}

// L1: one line. L2: one set, two ways. Latencies 1, 10, memory 100.
//   W A: L1 miss, L2 miss       1 + 10 + 100 = 111
//   R B: L1 miss, L2 miss       111; L1 evicts dirty A, written into L2 (a hit)
//   R A: L1 miss, L2 hit        1 + 10 = 11
//   R A: L1 hit                 1
// AMAT = 234 / 4 = 58.5 cycles.
static void two_level_amat() {
  Hierarchy h({{{64, 1, 64}, 1}, {{128, 2, 64}, 10}}, 100);
  EXPECT(h.access(0x000, true) == 111);
  EXPECT(h.access(0x040, false) == 111);
  EXPECT(h.access(0x000, false) == 11);
  EXPECT(h.access(0x000, false) == 1);
  EXPECT(h.amat() == 58.5);
  EXPECT(h.stats(0).writebacks == 1);
  EXPECT(h.stats(1).reads == 3 && h.stats(1).read_misses == 2);
  EXPECT(h.stats(1).writes == 1 && h.stats(1).write_misses == 0);
}

// One level (latency 1, 16 sets, no evictions), memory 100, degree 2.
// Reads of blocks 0-5; t is the cycle the L1 lookup finishes.
//   b0 t=1    miss, 101 cycles; no previous block
//   b1 t=102  miss, 101; stride 1 seen once
//   b2 t=203  miss, 101; stride 1 confirmed: prefetch b3, b4 (ready 303)
//   b3 t=304  prefetch hit, on time, 1; prefetch b5 (ready 404), b4 present
//   b4 t=305  prefetch hit, on time, 1; prefetch b6 (ready 405)
//   b5 t=306  prefetch hit, waits 98 for b5: late, 99; prefetch b7
// Issued 5 (b3-b7), useful 3, late 1, demand misses 3.
static void stride_prefetch() {
  Hierarchy h({{{4096, 4, 64}, 1}}, 100, 2);
  const uint64_t expected[] = {101, 101, 101, 1, 1, 99};
  for (uint64_t b = 0; b < 6; ++b) EXPECT(h.access(b * 64, false) == expected[b]);
  const PrefetchStats &p = h.prefetch_stats();
  EXPECT(p.issued == 5 && p.useful == 3 && p.late == 1 && p.demand_misses == 3);
  EXPECT(p.accuracy() == 0.6 && p.coverage() == 0.5);
}

static void rejects_bad_config() {
  bool threw = false;
  try {
    Cache c({1000, 2, 64});
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  EXPECT(threw);
}

int main() {
  same_line_hits();
  direct_mapped_conflict();
  lru_eviction();
  ways_of_reports_set_contents();
  dirty_writeback();
  srrip_keeps_reused_line_through_scan();
  brrip_resists_thrashing();
  two_level_amat();
  stride_prefetch();
  rejects_bad_config();
  if (failures) return 1;
  puts("PASS: cache-sim");
  return 0;
}
