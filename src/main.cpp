// cachesim [--size 32K --ways 8 --line 64 --policy lru|srrip|brrip]
//          [--l2-size 256K --l2-ways 8] [--l1-lat 4 --l2-lat 12 --mem-lat 200] <trace>
// --size/--ways describe L1; --l2-size adds an L2 with the same line size and
// policy. Latencies are in cycles.
// Trace: one access per line, "R <hex addr>" or "W <hex addr>"; '#' starts a comment.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "hierarchy.hpp"

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
  uint32_t l1_latency = 4, l2_latency = 12, memory_latency = 200;
  const char *path = nullptr;
  try {
    for (int i = 1; i < argc; ++i) {
      bool has_value = i + 1 < argc;
      if (!strcmp(argv[i], "--size") && has_value) l1.size_bytes = parse_size(argv[++i]);
      else if (!strcmp(argv[i], "--ways") && has_value) l1.ways = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--line") && has_value) l1.line_bytes = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--policy") && has_value) l1.policy = parse_policy(argv[++i]);
      else if (!strcmp(argv[i], "--l2-size") && has_value) l2.size_bytes = parse_size(argv[++i]);
      else if (!strcmp(argv[i], "--l2-ways") && has_value) l2.ways = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--l1-lat") && has_value) l1_latency = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--l2-lat") && has_value) l2_latency = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--mem-lat") && has_value) memory_latency = uint32_t(parse_size(argv[++i]));
      else if (argv[i][0] != '-' && !path) path = argv[i];
      else throw std::invalid_argument(std::string("bad argument: ") + argv[i]);
    }
    if (!path) throw std::invalid_argument("missing trace file");

    std::vector<Hierarchy::Level> levels{{l1, l1_latency}};
    if (l2.size_bytes) {
      l2.line_bytes = l1.line_bytes;
      l2.policy = l1.policy;
      levels.push_back({l2, l2_latency});
    }
    Hierarchy hierarchy(levels, memory_latency);
    std::ifstream in(path);
    if (!in) throw std::invalid_argument(std::string("cannot open ") + path);
    std::string line;
    for (uint64_t n = 1; std::getline(in, line); ++n) {
      std::istringstream fields(line.substr(0, line.find('#')));
      std::string op, addr;
      if (!(fields >> op)) continue;
      if (!(fields >> addr) || (op != "R" && op != "W"))
        throw std::invalid_argument(std::string(path) + ":" + std::to_string(n) + ": bad access");
      hierarchy.access(std::stoull(addr, nullptr, 16), op == "W");
    }

    // An L2 access is an L1 miss (read) or an L1 writeback (write).
    printf("level    accesses      misses  miss rate  writebacks\n");
    for (size_t i = 0; i < hierarchy.levels(); ++i) {
      const CacheStats &s = hierarchy.stats(i);
      printf("L%zu   %11llu %11llu %9.2f%% %11llu\n", i + 1, (unsigned long long)s.accesses(),
             (unsigned long long)s.misses(), s.accesses() ? 100.0 * s.misses() / s.accesses() : 0.0,
             (unsigned long long)s.writebacks);
    }
    printf("AMAT     %.2f cycles\n", hierarchy.amat());
  } catch (const std::exception &e) {
    fprintf(stderr, "cachesim: %s\n", e.what());
    return 2;
  }
  return 0;
}
