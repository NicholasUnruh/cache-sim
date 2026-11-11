// cachesim --size 32K --ways 8 --line 64 <trace>
// Trace: one access per line, "R <hex addr>" or "W <hex addr>"; '#' starts a comment.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "cache.hpp"

static uint64_t parse_size(const char *s) {
  char *end = nullptr;
  uint64_t n = strtoull(s, &end, 10);
  if (*end == 'K' || *end == 'k') return n << 10;
  if (*end == 'M' || *end == 'm') return n << 20;
  if (*end != '\0') throw std::invalid_argument(std::string("bad size: ") + s);
  return n;
}

int main(int argc, char **argv) {
  CacheConfig config{32 << 10, 8, 64};
  const char *path = nullptr;
  try {
    for (int i = 1; i < argc; ++i) {
      bool has_value = i + 1 < argc;
      if (!strcmp(argv[i], "--size") && has_value) config.size_bytes = parse_size(argv[++i]);
      else if (!strcmp(argv[i], "--ways") && has_value) config.ways = uint32_t(parse_size(argv[++i]));
      else if (!strcmp(argv[i], "--line") && has_value) config.line_bytes = uint32_t(parse_size(argv[++i]));
      else if (argv[i][0] != '-' && !path) path = argv[i];
      else throw std::invalid_argument(std::string("bad argument: ") + argv[i]);
    }
    if (!path) throw std::invalid_argument("missing trace file");

    Cache cache(config);
    std::ifstream in(path);
    if (!in) throw std::invalid_argument(std::string("cannot open ") + path);
    std::string line;
    for (uint64_t n = 1; std::getline(in, line); ++n) {
      std::istringstream fields(line.substr(0, line.find('#')));
      std::string op, addr;
      if (!(fields >> op)) continue;
      if (!(fields >> addr) || (op != "R" && op != "W"))
        throw std::invalid_argument(std::string(path) + ":" + std::to_string(n) + ": bad access");
      cache.access(std::stoull(addr, nullptr, 16), op == "W");
    }

    const CacheStats &s = cache.stats();
    printf("accesses    %llu\n", (unsigned long long)s.accesses());
    printf("misses      %llu\n", (unsigned long long)s.misses());
    printf("miss rate   %.2f%%\n", s.accesses() ? 100.0 * s.misses() / s.accesses() : 0.0);
    printf("writebacks  %llu\n", (unsigned long long)s.writebacks);
  } catch (const std::exception &e) {
    fprintf(stderr, "cachesim: %s\n", e.what());
    return 2;
  }
  return 0;
}
