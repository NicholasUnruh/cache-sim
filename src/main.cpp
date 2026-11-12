// cachesim: trace-driven cache and memory-hierarchy simulator. Usage text below.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "hierarchy.hpp"

static const char kUsage[] =
    "usage: cachesim [options] <trace>\n"
    "  --size 32K          L1 size (K or M suffix)\n"
    "  --ways 8            L1 associativity\n"
    "  --line 64           line size in bytes, shared by every level\n"
    "  --policy lru        replacement policy: lru, srrip or brrip (every level)\n"
    "  --l2-size 256K      add an L2 of this size (default: no L2)\n"
    "  --l2-ways 8         L2 associativity\n"
    "  --l1-lat 4          L1 hit latency, cycles\n"
    "  --l2-lat 12         L2 hit latency, cycles\n"
    "  --mem-lat 200       memory latency, cycles\n"
    "  --prefetch <deg>    stride prefetcher at the last level, <deg> blocks ahead\n"
    "  --events <file>     per access: L1 set, tag, hit/miss, evicted tag (* if dirty),\n"
    "                      set's tags\n"
    "  -h, --help          print this help\n"
    "trace: one access per line, \"R <hex addr>\" or \"W <hex addr>\"; '#' starts a comment\n";

static uint64_t parse_size(const char *s) {
  char *end = nullptr;
  uint64_t n = strtoull(s, &end, 10);
  if (*end == 'K' || *end == 'k') return n << 10;
  if (*end == 'M' || *end == 'm') return n << 20;
  if (*end != '\0') throw std::invalid_argument(std::string("bad size: ") + s);
  return n;
}

static Policy parse_policy(const char *s) {
  if (!strcmp(s, "lru")) return Policy::LRU;
  if (!strcmp(s, "srrip")) return Policy::SRRIP;
  if (!strcmp(s, "brrip")) return Policy::BRRIP;
  throw std::invalid_argument(std::string("bad policy (lru, srrip, brrip): ") + s);
}

int main(int argc, char **argv) {
  CacheConfig l1{32 << 10, 8, 64};
  CacheConfig l2{0, 8, 64};  // size 0: no L2
  uint32_t l1_latency = 4, l2_latency = 12, memory_latency = 200, prefetch_degree = 0;
  const char *path = nullptr;
  const char *events_path = nullptr;
  try {
    for (int i = 1; i < argc; ++i) {
      bool has_value = i + 1 < argc;
      if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
        fputs(kUsage, stdout);
        return 0;
      } else if (!strcmp(argv[i], "--size") && has_value)
        l1.size_bytes = parse_size(argv[++i]);
      else if (!strcmp(argv[i], "--ways") && has_value)
        l1.ways = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--line") && has_value)
        l1.line_bytes = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--policy") && has_value)
        l1.policy = parse_policy(argv[++i]);
      else if (!strcmp(argv[i], "--l2-size") && has_value)
        l2.size_bytes = parse_size(argv[++i]);
      else if (!strcmp(argv[i], "--l2-ways") && has_value)
        l2.ways = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--l1-lat") && has_value)
        l1_latency = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--l2-lat") && has_value)
        l2_latency = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--mem-lat") && has_value)
        memory_latency = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--prefetch") && has_value)
        prefetch_degree = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--events") && has_value)
        events_path = argv[++i];
      else if (argv[i][0] != '-' && !path)
        path = argv[i];
      else
        throw std::invalid_argument(std::string("bad argument: ") + argv[i]);
    }
    if (!path) throw std::invalid_argument("missing trace file");

    std::vector<Hierarchy::Level> levels{{l1, l1_latency}};
    if (l2.size_bytes) {
      l2.line_bytes = l1.line_bytes;
      l2.policy = l1.policy;
      levels.push_back({l2, l2_latency});
    }
    Hierarchy hierarchy(levels, memory_latency, prefetch_degree);
    std::ifstream in(path);
    if (!in) throw std::invalid_argument(std::string("cannot open ") + path);
    std::ofstream events;
    if (events_path) {
      events.open(events_path);
      if (!events) throw std::invalid_argument(std::string("cannot write ") + events_path);
      events << "# op addr set tag hit|miss evicted-tag[*=written back] way-tags... (hex; - = none)\n"
             << std::hex;
    }
    std::string line;
    for (uint64_t n = 1; std::getline(in, line); ++n) {
      std::istringstream fields(line.substr(0, line.find('#')));
      std::string op, addr;
      if (!(fields >> op)) continue;
      if (!(fields >> addr) || (op != "R" && op != "W"))
        throw std::invalid_argument(std::string(path) + ":" + std::to_string(n) + ": bad access");
      uint64_t a = std::stoull(addr, nullptr, 16);
      if (!events_path) {
        hierarchy.access(a, op == "W");
        continue;
      }
      const Cache &l1_cache = hierarchy.cache(0);
      std::vector<int64_t> before = l1_cache.ways_of(a);
      uint64_t misses = l1_cache.stats().misses();
      uint64_t writebacks = l1_cache.stats().writebacks;
      hierarchy.access(a, op == "W");
      std::vector<int64_t> after = l1_cache.ways_of(a);
      events << op << ' ' << a << ' ' << l1_cache.set_of(a) << ' ' << l1_cache.tag_of(a)
             << (l1_cache.stats().misses() == misses ? " H " : " M ");
      int64_t evicted = -1;
      for (size_t w = 0; w < before.size(); ++w)
        if (before[w] >= 0 && before[w] != after[w]) evicted = before[w];
      evicted < 0 ? events << '-' : events << evicted;
      if (l1_cache.stats().writebacks != writebacks) events << '*';
      for (int64_t tag : after) tag < 0 ? events << " -" : events << ' ' << tag;
      events << '\n';
    }

    // An L2 access is an L1 miss (read) or an L1 writeback (write).
    printf("level    accesses      misses  miss rate  writebacks\n");
    for (size_t i = 0; i < hierarchy.levels(); ++i) {
      const CacheStats &s = hierarchy.stats(i);
      printf("L%zu   %11llu %11llu %9.2f%% %11llu\n", i + 1, (unsigned long long)s.accesses(),
             (unsigned long long)s.misses(), s.accesses() ? 100.0 * s.misses() / s.accesses() : 0.0,
             (unsigned long long)s.writebacks);
    }
    if (prefetch_degree) {
      const PrefetchStats &p = hierarchy.prefetch_stats();
      printf("prefetch issued %llu  useful %llu  late %llu\n", (unsigned long long)p.issued,
             (unsigned long long)p.useful, (unsigned long long)p.late);
      printf("prefetch accuracy %.2f%%  coverage %.2f%%  lateness %.2f%%\n", 100 * p.accuracy(),
             100 * p.coverage(), 100 * p.lateness());
    }
    printf("AMAT     %.2f cycles\n", hierarchy.amat());
  } catch (const std::exception &e) {
    fprintf(stderr, "cachesim: %s (see --help)\n", e.what());
    return 2;
  }
  return 0;
}
