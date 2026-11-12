"""Render the figures in docs/img from the JSON written by tools/cache_steps.py
and tools/sweep.py, and the real-program table in docs/traces.md.

Needs matplotlib. Run from the repo root after those scripts:
  uv run --with matplotlib python tools/plot.py docs/img          # light PNGs
  uv run --with matplotlib python tools/plot.py OUT --theme dark  # dark PNGs
Also writes build/amat.json, the docs/traces.md table as data.
"""

import argparse
import json
import re
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.colors import LinearSegmentedColormap  # noqa: E402

THEMES = {
    "light": {
        "bg": "#fcfcfb",
        "ink": "#0b0b0b",
        "muted": "#52514e",
        "grid": "#e4e3df",
        "series": ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"],
        "hit": "#b9b8b2",
        "miss": "#eb6834",
        "ramp": ["#f3f7fd", "#9ec5f4", "#2a78d6", "#0d366b"],
    },
    "dark": {
        "bg": "#0a0b12",
        "ink": "#f2f2f0",
        "muted": "#9a9a98",
        "grid": "#24252e",
        "series": ["#3987e5", "#f07a45", "#199e70", "#c98500"],
        "hit": "#4a4b55",
        "miss": "#f07a45",
        "ramp": ["#10121c", "#184f95", "#5598e7", "#cde2fb"],
    },
}


def style(t):
    plt.rcParams.update(
        {
            "figure.facecolor": t["bg"],
            "axes.facecolor": t["bg"],
            "savefig.facecolor": t["bg"],
            "text.color": t["ink"],
            "axes.labelcolor": t["muted"],
            "axes.edgecolor": t["grid"],
            "xtick.color": t["muted"],
            "ytick.color": t["muted"],
            "axes.titlecolor": t["ink"],
            "font.family": "DejaVu Sans",
            "font.size": 10,
            "axes.spines.top": False,
            "axes.spines.right": False,
        }
    )


def steps(t, out):
    fig, axes = plt.subplots(2, 1, figsize=(11, 5.6), sharex=True)
    for ax, name in zip(axes, ["ijk", "tiled"]):
        d = json.loads(Path(f"build/cache-steps-{name}.json").read_text())
        hits = [(i, s[2]) for i, s in enumerate(d["steps"]) if s[4]]
        misses = [(i, s[2]) for i, s in enumerate(d["steps"]) if not s[4]]
        ax.scatter(*zip(*hits), s=10, color=t["hit"], label="hit", linewidths=0)
        ax.scatter(*zip(*misses), s=22, color=t["miss"], label="miss", linewidths=0)
        c, w = d["config"], d["window"]
        ax.set_title(
            f"{d['trace'].split(' ', 1)[1]}: accesses {w['start']}–{w['start'] + w['count'] - 1}, "
            f"{100 * w['hitRate']:.2f}% hit (whole trace {100 * d['full']['hitRate']:.2f}%)",
            loc="left",
            fontsize=10,
        )
        ax.set_ylim(-0.7, c["sets"] - 0.3)
        ax.set_yticks(range(0, c["sets"], 4))
        ax.set_ylabel("set")
        ax.invert_yaxis()
    axes[0].legend(loc="upper right", frameon=False, ncol=2, bbox_to_anchor=(1, 1.22))
    axes[1].set_xlabel("access in window")
    fig.suptitle(
        f"Which L1 set each access touches ({c['size']}B, {c['sets']} sets × {c['ways']} ways, {c['line']} B lines, LRU)",
        x=0.01,
        ha="left",
        fontsize=11,
    )
    fig.tight_layout()
    fig.savefig(out / "cache-steps.png", dpi=130)


def heatmap(t, out):
    d = json.loads(Path("build/heatmap.json").read_text())
    cmap = LinearSegmentedColormap.from_list("seq", t["ramp"])
    fig, axes = plt.subplots(1, 2, figsize=(10, 4.6))
    for ax, (name, grid) in zip(axes, d["missRate"].items()):
        ax.imshow(grid, cmap=cmap, vmin=0, vmax=55, aspect="auto")
        for r, row in enumerate(grid):
            for c, v in enumerate(row):
                ax.text(
                    c,
                    r,
                    f"{v:.2f}",
                    ha="center",
                    va="center",
                    fontsize=8.5,
                    color=t["bg"] if v > 25 else t["ink"],
                )
        ax.set_xticks(range(len(d["ways"])), [f"{w}-way" for w in d["ways"]])
        ax.set_yticks(range(len(d["sizes"])), [s + "B" for s in d["sizes"]])
        ax.set_title(name, loc="left")
        ax.tick_params(length=0)
        for s in ax.spines.values():
            s.set_visible(False)
    axes[0].set_ylabel("L1 size")
    fig.suptitle("L1 miss rate (%), 64 B lines, LRU", x=0.01, ha="left", fontsize=11)
    fig.tight_layout()
    fig.savefig(out / "heatmap.png", dpi=130)


def amat(t, out):
    rows = [
        line.split("|")[1:-1]
        for line in Path("docs/traces.md").read_text().splitlines()
        if re.match(r"\| `", line)
    ]
    policies = ["LRU", "SRRIP", "BRRIP", "LRU + prefetch 4"]
    data = {
        "source": "docs/traces.md",
        "config": "L1 32 KB, L2 256 KB, both 8-way, 64 B lines, latencies 4/12/200; AMAT cycles",
        "policies": policies,
        "programs": {
            r[0].strip().replace("`", ""): [float(v) for v in r[4:8]] for r in rows
        },
    }
    Path("build/amat.json").write_text(json.dumps(data, indent=1))
    fig, ax = plt.subplots(figsize=(10, 4.4))
    names = list(data["programs"])
    width = 0.2
    for k, p in enumerate(policies):
        xs = [i + (k - 1.5) * width for i in range(len(names))]
        ys = [data["programs"][n][k] for n in names]
        ax.bar(xs, ys, width * 0.9, color=t["series"][k], label=p)
    ax.set_xticks(range(len(names)), names)
    ax.set_ylabel("AMAT (cycles)")
    ax.yaxis.grid(True, color=t["grid"])
    ax.set_axisbelow(True)
    ax.tick_params(length=0)
    ax.legend(frameon=False, ncol=4, loc="upper left", bbox_to_anchor=(0, 1.12))
    for i, n in enumerate(names):  # label the best bar per program
        best = min(range(4), key=lambda k: data["programs"][n][k])
        ax.text(
            i + (best - 1.5) * width,
            data["programs"][n][best] + 0.1,
            f"{data['programs'][n][best]:.2f}",
            ha="center",
            fontsize=8,
            color=t["ink"],
        )
    fig.suptitle(
        "Real programs (Valgrind traces): AMAT by policy",
        x=0.01,
        ha="left",
        fontsize=11,
        y=0.99,
    )
    fig.tight_layout()
    fig.savefig(out / "amat.png", dpi=130)


def features(t, out):
    d = json.loads(Path("build/features.json").read_text())
    # RRIP: miss rate per policy on three traces.
    fig, ax = plt.subplots(figsize=(7, 3.6))
    rows = d["rrip"]["rows"]
    for k, p in enumerate(["lru", "srrip", "brrip"]):
        xs = [i + (k - 1) * 0.26 for i in range(len(rows))]
        ys = [r[p] for r in rows.values()]
        ax.bar(xs, ys, 0.24, color=t["series"][k], label=p.upper())
        for x, y in zip(xs, ys):
            ax.text(x, y + 1, f"{y:.2f}", ha="center", fontsize=7.5, color=t["ink"])
    ax.set_xticks(range(len(rows)), list(rows))
    ax.set_ylabel("L1 miss rate (%)")
    ax.legend(frameon=False, ncol=3, loc="upper left", bbox_to_anchor=(0, 1.15))
    ax.tick_params(length=0)
    fig.suptitle(
        d["rrip"]["config"].split(";")[0], x=0.01, ha="left", fontsize=10, y=0.99
    )
    fig.tight_layout()
    fig.savefig(out / "rrip.png", dpi=130)
    plt.close(fig)

    # Hierarchy: AMAT with and without an L2.
    fig, ax = plt.subplots(figsize=(7, 3.6))
    rows = d["hierarchy"]["rows"]
    for k, key in enumerate(["L1 only", "L1+L2"]):
        xs = [i + (k - 0.5) * 0.36 for i in range(len(rows))]
        ys = [r[key] for r in rows.values()]
        ax.bar(xs, ys, 0.34, color=t["series"][k], label=key)
        for x, y in zip(xs, ys):
            ax.text(x, y + 3, f"{y:.2f}", ha="center", fontsize=8, color=t["ink"])
    ax.set_xticks(range(len(rows)), list(rows))
    ax.set_ylabel("AMAT (cycles)")
    ax.legend(frameon=False, ncol=2, loc="upper left", bbox_to_anchor=(0, 1.15))
    ax.tick_params(length=0)
    fig.suptitle(
        "L1 32 KB + L2 256 KB, latencies 4/12/200",
        x=0.01,
        ha="left",
        fontsize=10,
        y=0.99,
    )
    fig.tight_layout()
    fig.savefig(out / "hierarchy.png", dpi=130)
    plt.close(fig)

    # Prefetch: AMAT by degree on a sequential stream.
    fig, ax = plt.subplots(figsize=(7, 3.4))
    p = d["prefetch"]
    labels = ["off" if g == 0 else f"D={g}" for g in p["degrees"]]
    ax.bar(labels, p["seq64"], 0.5, color=t["series"][0])
    for i, y in enumerate(p["seq64"]):
        ax.text(i, y + 4, f"{y:.2f}", ha="center", fontsize=9, color=t["ink"])
    ax.set_ylabel("AMAT (cycles)")
    ax.tick_params(length=0)
    fig.suptitle(
        "Stride prefetcher, 4 MB sequential read (one access per line)",
        x=0.01,
        ha="left",
        fontsize=10,
    )
    fig.tight_layout()
    fig.savefig(out / "prefetch.png", dpi=130)
    plt.close(fig)


if __name__ == "__main__":
    a = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    a.add_argument("out")
    a.add_argument("--theme", choices=list(THEMES), default="light")
    args = a.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    t = THEMES[args.theme]
    style(t)
    for f in (steps, heatmap, amat, features):
        f(t, out)
        plt.close("all")
