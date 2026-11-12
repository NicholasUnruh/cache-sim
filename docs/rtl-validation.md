# Validation against RTL

cachesim's L1 is checked access by access against the data cache of [rv32i-pipeline](https://github.com/NicholasUnruh/rv32i-pipeline), a write-back, write-allocate LRU cache in SystemVerilog. The two were written separately from the same rules, so any disagreement is a bug.

## How

- The RTL testbench writes the core's data accesses as a trace (`R`/`W` and hex address) and, per access, what the RTL cache did, in the format of `cachesim --events`.
- `--events` gives, per access: set, tag, hit or miss, the evicted tag with `*` if the victim was dirty and written back, and each way's tag afterwards.
- rv32i-pipeline's `scripts/crosscheck.py` replays the trace here with `--policy lru` and the RTL's size, ways and line, then compares counts and diffs the event files line by line.

Matching events is stricter than matching counts: it also checks which way each line fills and the LRU order.

## Result

328 runs: a RISC-V C benchmark and all 40 rv32ui tests, each at eight geometries from 2 sets × 1 way × 8 B to 64 sets × 2 ways × 64 B. 60,032 accesses; misses, writebacks and every event line agree. On 14 Embench-IoT programs at five geometries, 55.7 million accesses also agree ([design space](rtl-design-space.md)). Tables: rv32i-pipeline `docs/7-crosscheck.md`.

To check the check, two replacement bugs were injected into the RTL. Filling the last invalid way instead of the first left every count unchanged but changed all 6,694 event lines. Not making a refilled line most recently used changed the counts too (43 vs 42 misses).

## Limits

- The RTL cache has no RRIP or prefetcher, so only single-level LRU behaviour is cross-checked.
- Timing is not compared: cachesim's AMAT is a blocking-core model, while the RTL's miss penalty is a fixed number of pipeline cycles.
