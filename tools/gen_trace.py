"""Write synthetic memory traces for cachesim to stdout.

stride:  read `bytes` of memory `passes` times, one access every `stride` bytes.
scan:    `rounds` times: read a `hot` byte working set `reuse` times, then
         scan `bytes` of memory that is never touched again.
random:  `count` reads at uniform random addresses inside `bytes`.
matmul:  the loads and stores of C = A * B on n x n float matrices, in loop
         order ijk, ikj or tiled (ijk over `tile` x `tile` blocks).
"""

import argparse
import random
import sys

A, B, C = 0x1000_0000, 0x2000_0000, 0x3000_0000
FLOAT = 4


def stride(out, size, step, passes):
    for _ in range(passes):
        for addr in range(0, size, step):
            out.write(f"R {addr:x}\n")


def scan(out, hot, reuse, size, rounds):
    line = 64
    for r in range(rounds):
        for _ in range(reuse):
            for addr in range(0, hot, line):
                out.write(f"R {addr:x}\n")
        base = A + r * size  # fresh memory every round, far from the hot set
        for addr in range(base, base + size, line):
            out.write(f"R {addr:x}\n")


def rand(out, size, count, seed):
    rng = random.Random(seed)
    for _ in range(count):
        out.write(f"R {rng.randrange(0, size, FLOAT):x}\n")


def matmul(out, n, order, tile):
    def a(i, k):
        return A + (i * n + k) * FLOAT

    def b(k, j):
        return B + (k * n + j) * FLOAT

    def c(i, j):
        return C + (i * n + j) * FLOAT

    w = out.write
    if order == "ijk":
        for i in range(n):
            for j in range(n):
                for k in range(n):
                    w(f"R {a(i, k):x}\nR {b(k, j):x}\n")
                w(f"W {c(i, j):x}\n")
    elif order == "ikj":
        for i in range(n):
            for k in range(n):
                w(f"R {a(i, k):x}\n")
                for j in range(n):
                    w(f"R {b(k, j):x}\nR {c(i, j):x}\nW {c(i, j):x}\n")
    else:
        for i0 in range(0, n, tile):
            for j0 in range(0, n, tile):
                for k0 in range(0, n, tile):
                    for i in range(i0, min(i0 + tile, n)):
                        for j in range(j0, min(j0 + tile, n)):
                            for k in range(k0, min(k0 + tile, n)):
                                w(f"R {a(i, k):x}\nR {b(k, j):x}\n")
                            w(f"R {c(i, j):x}\nW {c(i, j):x}\n")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="kind", required=True)
    s = sub.add_parser("stride")
    s.add_argument("--bytes", type=int, required=True)
    s.add_argument("--stride", type=int, default=64)
    s.add_argument("--passes", type=int, default=2)
    sc = sub.add_parser("scan")
    sc.add_argument("--hot", type=int, required=True)
    sc.add_argument("--reuse", type=int, default=2)
    sc.add_argument("--bytes", type=int, required=True)
    sc.add_argument("--rounds", type=int, default=20)
    r = sub.add_parser("random")
    r.add_argument("--bytes", type=int, required=True)
    r.add_argument("--count", type=int, default=100_000)
    r.add_argument("--seed", type=int, default=1)
    m = sub.add_parser("matmul")
    m.add_argument("--n", type=int, default=64)
    m.add_argument("--order", choices=["ijk", "ikj", "tiled"], default="ijk")
    m.add_argument("--tile", type=int, default=16)
    args = p.parse_args()

    if args.kind == "stride":
        stride(sys.stdout, args.bytes, args.stride, args.passes)
    elif args.kind == "scan":
        scan(sys.stdout, args.hot, args.reuse, args.bytes, args.rounds)
    elif args.kind == "random":
        rand(sys.stdout, args.bytes, args.count, args.seed)
    else:
        matmul(sys.stdout, args.n, args.order, args.tile)


if __name__ == "__main__":
    main()
