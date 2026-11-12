"""Run cachesim sweeps behind the figures in docs/img and write them as JSON.

heatmap:  L1 miss rate over size x associativity (64 B lines, LRU) for
          128x128 matmul ijk and tiled (32x32 tiles).
features: one measured row per feature (RRIP, L2 + AMAT, prefetch degree),
          rerun on the configs of docs/rrip.md, hierarchy.md and prefetch.md.
Run from the repo root after `make build/cachesim`:
  python3 tools/sweep.py heatmap > build/heatmap.json
  python3 tools/sweep.py features > build/features.json
"""

import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor


def trace(name, *gen):
    path = f"build/sweep-{name}.trace"
    with open(path, "w") as f:
        subprocess.run(
            [sys.executable, "tools/gen_trace.py", *gen],
            stdout=f,
            check=True,
            timeout=600,
        )
    return path


def run(path, *opts):
    """Returns {"L1": miss %, "L2": miss %, "AMAT": cycles} from one cachesim run."""
    out = subprocess.run(
        ["build/cachesim", *opts, path],
        capture_output=True,
        text=True,
        check=True,
        timeout=600,
    ).stdout
    r = {}
    for line in out.splitlines():
        f = line.split()
        if f[0] in ("L1", "L2"):
            r[f[0]] = float(f[3].rstrip("%"))
        elif f[0] == "AMAT":
            r["AMAT"] = float(f[1])
    return r


def heatmap():
    sizes = ["4K", "8K", "16K", "32K", "64K", "128K", "256K"]
    ways = [1, 2, 4, 8, 16]
    traces = {
        "matmul 128 ijk": trace("ijk128", "matmul", "--n", "128", "--order", "ijk"),
        "matmul 128 tiled 32": trace(
            "tiled128", "matmul", "--n", "128", "--order", "tiled", "--tile", "32"
        ),
    }
    jobs = [(t, s, w) for t in traces for s in sizes for w in ways]
    with ThreadPoolExecutor(4) as pool:
        rates = list(
            pool.map(
                lambda j: run(
                    traces[j[0]], "--size", j[1], "--ways", str(j[2]), "--line", "64"
                )["L1"],
                jobs,
            )
        )
    grid = {t: [[0.0] * len(ways) for _ in sizes] for t in traces}
    for (t, s, w), r in zip(jobs, rates):
        grid[t][sizes.index(s)][ways.index(w)] = r
    return {
        "metric": "L1 miss rate %, 64 B lines, LRU",
        "sizes": sizes,
        "ways": ways,
        "missRate": grid,
    }


def features():
    l1_16k = ["--size", "16K", "--ways", "4", "--line", "64"]
    two = [
        "--size",
        "32K",
        "--ways",
        "8",
        "--line",
        "64",
        "--l2-size",
        "256K",
        "--l2-ways",
        "8",
    ]
    rrip = {
        "matmul 64 ijk": trace("ijk64", "matmul", "--n", "64", "--order", "ijk"),
        "scan 8/16": trace("scan", "scan", "--hot", "8192", "--bytes", "16384"),
        "2 passes, 32 KB": trace(
            "passes", "stride", "--bytes", "32768", "--passes", "2"
        ),
    }
    hier = {
        "matmul 128 ijk": trace("ijk128", "matmul", "--n", "128", "--order", "ijk"),
        "matmul 256 tiled": trace(
            "tiled256", "matmul", "--n", "256", "--order", "tiled", "--tile", "32"
        ),
        "random 1 MB": trace(
            "random", "random", "--bytes", "1048576", "--count", "2000000"
        ),
    }
    seq64 = trace(
        "seq64", "stride", "--bytes", str(4 << 20), "--stride", "64", "--passes", "1"
    )
    return {
        "rrip": {
            "config": "L1 16 KB 4-way, 64 B lines; L1 miss rate %",
            "rows": {
                name: {
                    p: run(path, *l1_16k, "--policy", p)["L1"]
                    for p in ("lru", "srrip", "brrip")
                }
                for name, path in rrip.items()
            },
        },
        "hierarchy": {
            "config": "L1 32 KB 8-way, L2 256 KB 8-way, 64 B lines, LRU, latencies 4/12/200; AMAT cycles",
            "rows": {
                name: {
                    "L1 only": run(path, *two[:6])["AMAT"],
                    "L1+L2": run(path, *two)["AMAT"],
                }
                for name, path in hier.items()
            },
        },
        "prefetch": {
            "config": "same hierarchy, seq64 = 4 MB read once, one access per line; AMAT cycles by degree",
            "degrees": [0, 1, 4, 16],
            "seq64": [
                run(seq64, *two, *(["--prefetch", str(d)] if d else []))["AMAT"]
                for d in (0, 1, 4, 16)
            ],
        },
    }


if __name__ == "__main__":
    kind = sys.argv[1] if len(sys.argv) == 2 else ""
    if kind not in ("heatmap", "features"):
        sys.exit(__doc__)
    json.dump(heatmap() if kind == "heatmap" else features(), sys.stdout, indent=1)
