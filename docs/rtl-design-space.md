# Choosing the RTL cache

cachesim chose the data cache of [rv32i-pipeline](https://github.com/NicholasUnruh/rv32i-pipeline), and the RTL then confirmed the choice.

## How

- The core runs 14 Embench-IoT programs and writes each one's data trace. The trace is the same for any cache geometry.
- rv32i-pipeline's `scripts/dcache_sweep.py` replays every trace here at 240 points: 256 B to 16 KB, 1 to 8 ways, 16 to 64 B lines, under LRU, SRRIP and BRRIP.
- Misses and dirty writebacks become cycles through the core's fixed stall: latency + W + 3 per miss, W + 1 per dirty victim, where W is words per line.

## Result

- Size dominates. Geometric-mean CPI falls from 2.103 at 1 KB to 1.906 at 8 KB, where nettle-aes's two 4.25 KB AES tables fit.
- From 1 KB up, more ways change the mean by less than 0.5%, and SRRIP or BRRIP beat LRU by at most 0.4%. Below 1 KB, SRRIP gains up to 1.2%.
- Chosen: the smallest size within 1% of the best, the fewest ways, then the lowest CPI. That is 8 KB, 2-way, 64 B lines, LRU.
- Rebuilt with it, the RTL's misses, writebacks and stall cycles match cachesim exactly on all 14 programs.

## Limit found

Total cycles came out up to 1.8% above the prediction. Instruction fills continue while a data miss stalls the core, so part of their latency was hidden under data stalls that the larger cache removed. A single-cache model can't see overlap between two caches. Details: rv32i-pipeline `docs/8-embench.md`.
