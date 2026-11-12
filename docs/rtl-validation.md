# Validation against RTL

cachesim's L1 is checked access by access against the data cache of [rv32i-pipeline](https://github.com/NicholasUnruh/rv32i-pipeline), a write-back, write-allocate LRU cache in SystemVerilog. The two were written separately from the same rules, so every disagreement is a bug in one of them.

## How

- The RTL testbench writes the core's data accesses as a trace (`R`/`W` and hex address) and, per access, what the RTL cache did, in the format of `cachesim --events`.
- `--events` gives, per access: set, tag, hit or miss, the evicted tag with `*` if the victim was dirty and written back, and each way's tag afterwards.
- rv32i-pipeline's `scripts/crosscheck.py` replays the trace here with `--policy lru` and the RTL's size, ways and line, then compares counts and diffs the event files line by line.

Matching events is stricter than matching miss counts: it also checks which way each line fills (the first invalid way, then the least recently used) and the LRU order.

## Result

287 runs: a RISC-V C benchmark and all 40 rv32ui tests, each at seven geometries from 2 sets × 1 way × 8 B to 32 sets × 2 ways × 16 B. 52,528 accesses; misses, writebacks and every event line agree. Tables: rv32i-pipeline `docs/7-crosscheck.md`.

To check the check, two replacement bugs were injected into the RTL. Filling the last invalid way instead of the first left every count unchanged (42 misses, 10 writebacks in both) but changed all 6,694 event lines. Not making a refilled line most recently used changed the counts too (43 vs 42 misses).

## Limits

- The RTL cache has no RRIP or prefetcher, so only single-level LRU behaviour is cross-checked.
- Timing is not compared: cachesim's AMAT is a blocking-core model, while the RTL's miss penalty is a fixed number of pipeline cycles.
