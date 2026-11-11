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
  rejects_bad_config();
  if (failures) return 1;
  puts("PASS: cache-sim");
  return 0;
}
