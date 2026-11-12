# Cache Simulator

- Goal: a portfolio project for computer-architecture internships. Agents write the policies, prefetcher and hierarchy (owner decision 2026-11-11); keep the code readable enough for the owner to explain in an interview.
- `src/cache.{hpp,cpp}` is the cache, `src/main.cpp` the CLI, `tools/gen_trace.py` the synthetic traces (no dependencies).
- Run `make check` before committing: unit tests plus a smoke run where LRU must miss every access.
- Every new policy gets a hand-checked unit test in `tests/`, measured rows in the README or its `docs/` note (run `cachesim`, never estimate) and a `docs/` note of at most 300 words on why it wins or loses per trace.
- Compare policies on identical traces and configs. Traces are generated, not committed (`*.trace` is ignored).
- Keep the README at or below 200 words and each `docs/` file at or below 500.
