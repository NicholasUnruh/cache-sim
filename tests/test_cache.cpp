#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "../src/cache.hpp"

static int failures = 0;
#define EXPECT(cond)                                                \
  do {                                                              \
    if (!(cond)) {                                                  \
      fprintf(stderr, "%s:%d: FAILED %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                   \
    }                                                               \
  } while (0)

static void same_line_hits() {
  Cache c({1024, 2, 64});
  EXPECT(!c.access(0x100, false));
  EXPECT(c.access(0x100, false));
  EXPECT(c.access(0x13f, false));  // last byte of the same line
  EXPECT(!c.access(0x140, false));
}

static void direct_mapped_conflict() {
  Cache c({256, 1, 64});  // 4 sets: 0x000 and 0x100 share set 0
  EXPECT(!c.access(0x000, false));
  EXPECT(!c.access(0x100, false));
  EXPECT(!c.access(0x000, false));
  EXPECT(c.stats().misses() == 3);
}

static void lru_eviction() {
  Cache c({128, 2, 64});  // one set, two ways
  EXPECT(!c.access(0x000, false));  // A
  EXPECT(!c.access(0x040, false));  // B
  EXPECT(c.access(0x000, false));   // A is now most recent
  EXPECT(!c.access(0x080, false));  // C evicts B
  EXPECT(c.access(0x000, false));
  EXPECT(!c.access(0x040, false));
}

static void dirty_writeback() {
  Cache c({64, 1, 64});  // one line
  EXPECT(!c.access(0x000, true));
  EXPECT(!c.access(0x040, false));  // evicts the dirty line
  EXPECT(!c.access(0x000, false));  // evicts a clean line
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
  dirty_writeback();
  srrip_keeps_reused_line_through_scan();
  brrip_resists_thrashing();
  rejects_bad_config();
  if (failures) return 1;
  puts("PASS: cache-sim");
  return 0;
}
