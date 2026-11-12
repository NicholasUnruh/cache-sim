# Real program traces

`tools/real_traces.sh` runs six programs under Valgrind Lackey (`--trace-mem=yes`), converts loads and stores with `tools/lackey2trace.py` (instruction fetches dropped), and prints the table below. Traces (0.002–3 GB each) stay in `build/traces`, out of Git. Captured with Valgrind 3.26 on Ubuntu 26.04 (x86-64); inputs are 1 MB of `/usr/include` headers (gzip, bzip2, xz, grep) and 200,000 random integers (sort). Each trace includes the program's start-up.

L1 32 KB, L2 256 KB, both 8-way, 64 B lines, latencies 4/12/200. AMAT in cycles.

| Program | Accesses | L1 miss | L2 miss | LRU | SRRIP | BRRIP | LRU + prefetch 4 |
|---|---:|---:|---:|---:|---:|---:|---:|
| `gzip -6` | 28.9 M | 12.95% | 0.40% | 5.68 | 5.64 | 5.90 | 5.64 |
| `bzip2 -9` | 155.5 M | 3.52% | 43.53% | 8.34 | 8.37 | 8.99 | 8.03 |
| `xz -1` | 77.3 M | 2.94% | 23.71% | 6.58 | 6.54 | 6.77 | 6.44 |
| `sort -n` | 265.4 M | 0.82% | 53.80% | 5.32 | 5.34 | 5.49 | 5.26 |
| `grep -cE` | 0.18 M | 2.92% | 67.60% | 9.09 | 9.09 | 9.10 | 6.74 |
| `python3` dict | 145.4 M | 2.67% | 55.99% | 8.56 | 8.51 | 8.67 | 7.38 |

Prefetch (D=4) coverage: grep 55%, python 29%, gzip 29%, bzip2 9%, xz 7%, sort 4%.

- **gzip**: its hash chains and window miss the 32 KB L1 often but fit in L2 (0.40% misses), so replacement and prefetching hardly matter.
- **SRRIP** stays within 0.05 cycles of LRU on every program, so these traces have little of the scan-over-hot-set pattern it protects against.
- **BRRIP** loses on every program. Inserting at "distant" evicts new lines before their reuse (gzip L1 misses 12.95% → 14.93%, xz 2.94% → 3.77%). Real designs only use it inside DRRIP, which duels BRRIP against SRRIP per set.
- **Prefetching** pays where the misses are a sequential scan: grep's input buffer (L2 misses 67.60% → 30.80%) and python (55.99% → 39.88%). bzip2's suffix sorting and sort's merges touch memory in data-dependent order, so a single stride stream finds little.
