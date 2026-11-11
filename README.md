# Cache Simulator

A trace-driven cache and memory-hierarchy simulator in C++17 for comparing replacement policies, prefetchers and multi-level hierarchies on real and synthetic memory traces.

The baseline is a set-associative, write-back, write-allocate cache with true LRU. Next come RRIP replacement, a stride prefetcher, an L1/L2 hierarchy with average memory access time, and real program traces.

```sh
make check
python3 tools/gen_trace.py matmul --n 64 --order ijk > build/ijk.trace
./build/cachesim --size 16K --ways 4 --line 64 build/ijk.trace
```

Trace format: one access per line, `R <hex address>` or `W <hex address>`.

| Workload (16 KB, 4-way, 64 B lines) | LRU miss rate |
|---|---:|
| 64×64 matmul, `ijk` | 4.90% |
| 64×64 matmul, `ikj` | 0.35% |
| 64×64 matmul, 16×16 tiles | 0.32% |
| Two passes over 2× cache size | 100.00% |
