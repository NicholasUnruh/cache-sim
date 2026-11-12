"""Dump a window of per-access cache events as JSON for a steppable set view.

Generates a synthetic trace with tools/gen_trace.py, runs build/cachesim with
--events (the events come from the simulator's own L1), and writes a window
of them plus the state of every set at the window's start. Run from the repo
root after `make build/cachesim`:
  python3 tools/cache_steps.py --order ijk --start 8256 --count 320 > build/cache-steps-ijk.json
  python3 tools/cache_steps.py --order tiled --tile 8 --start 10368 --count 320 > build/cache-steps-tiled.json
Each step: [op, addr, set, tag, hit, evicted tag or null, tags per way (null = empty)].
"""

import argparse
import json
import subprocess
import sys

p = argparse.ArgumentParser(
    description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
)
p.add_argument("--n", type=int, default=64)
p.add_argument("--order", choices=["ijk", "ikj", "tiled"], required=True)
p.add_argument("--tile", type=int, default=16)
p.add_argument("--size", default="4K")
p.add_argument("--ways", type=int, default=4)
p.add_argument("--line", type=int, default=64)
p.add_argument(
    "--start", type=int, default=0, help="first access of the window (0-based)"
)
p.add_argument("--count", type=int, default=320)
args = p.parse_args()

gen = ["matmul", "--n", str(args.n), "--order", args.order] + (
    ["--tile", str(args.tile)] if args.order == "tiled" else []
)
trace, events = f"build/steps-{args.order}.trace", f"build/steps-{args.order}.events"
with open(trace, "w") as f:
    subprocess.run(
        [sys.executable, "tools/gen_trace.py", *gen], stdout=f, check=True, timeout=600
    )
cfg = [
    "--size",
    args.size,
    "--ways",
    str(args.ways),
    "--line",
    str(args.line),
    "--policy",
    "lru",
]
out = subprocess.run(
    ["build/cachesim", *cfg, "--events", events, trace],
    capture_output=True,
    text=True,
    check=True,
    timeout=600,
).stdout
l1 = next(line.split() for line in out.splitlines() if line.startswith("L1"))
accesses, misses = int(l1[1]), int(l1[2])


def none_if_dash(s):
    return None if s == "-" else s


sets = int(args.size.rstrip("Kk")) * 1024 // (args.ways * args.line)
state = [[None] * args.ways for _ in range(sets)]  # after-state last seen per set
set_misses = [0] * sets
steps = []
with open(events) as f:
    rows = (line.split() for line in f if not line.startswith("#"))
    for i, (op, addr, s, tag, hm, ev, *ways) in enumerate(rows):
        s = int(s, 16)
        set_misses[s] += hm == "M"
        if args.start <= i < args.start + args.count:
            if not steps:
                initial = [list(w) for w in state]
            steps.append(
                [
                    op,
                    addr,
                    s,
                    tag,
                    int(hm == "H"),
                    none_if_dash(ev.rstrip("*")),
                    [none_if_dash(w) for w in ways],
                ]
            )
        state[s] = [none_if_dash(w) for w in ways]

hits = sum(st[4] for st in steps)
json.dump(
    {
        "trace": "gen_trace.py " + " ".join(gen),
        "config": {
            "size": args.size,
            "sets": sets,
            "ways": args.ways,
            "line": args.line,
            "policy": "lru",
        },
        "full": {
            "accesses": accesses,
            "misses": misses,
            "hitRate": round(1 - misses / accesses, 4),
        },
        "window": {
            "start": args.start,
            "count": len(steps),
            "hits": hits,
            "hitRate": round(hits / len(steps), 4),
        },
        "setMisses": set_misses,
        "initial": initial,
        "steps": steps,
    },
    sys.stdout,
    separators=(",", ":"),
)
