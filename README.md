# Cache Simulator

A trace-driven cache and memory-hierarchy simulator in C++17 for comparing replacement policies, prefetchers and multi-level hierarchies on real and synthetic memory traces.

The cache is set-associative, write-back and write-allocate, with LRU, SRRIP or BRRIP replacement ([docs/rrip.md](docs/rrip.md)). Next come a stride prefetcher, an L1/L2 hierarchy with average memory access time, and real program traces.

```sh
make check
python3 tools/gen_trace.py matmul --n 64 --order ijk > build/ijk.trace
./build/cachesim --size 16K --ways 4 --line 64 --policy srrip build/ijk.trace
```

Trace format: one access per line, `R <hex address>` or `W <hex address>`.

| Miss rate (16 KB, 4-way, 64 B lines) | LRU | SRRIP | BRRIP |
|---|---:|---:|---:|
| 64×64 matmul, `ijk` | 4.90% | 4.34% | 6.83% |
| 64×64 matmul, 16×16 tiles | 0.32% | 0.32% | 0.33% |
| Two passes over 2× cache size | 100.00% | 100.00% | 81.45% |
| 8 KB hot set + 16 KB scans | 75.00% | 51.25% | 51.25% |
