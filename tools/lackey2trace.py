"""Convert Valgrind Lackey output (--tool=lackey --trace-mem=yes) on stdin to
a cachesim trace on stdout.

Lackey prints one line per memory event: "I  addr,size" for an instruction
fetch, " L addr,size" for a load, " S addr,size" for a store and
" M addr,size" for a modify (load then store). Instruction fetches and
Valgrind's own "==pid==" messages are dropped: cachesim models a data cache.
An access that straddles two lines is counted once, at its first byte.
"""

import sys


def main():
    out = sys.stdout
    for line in sys.stdin:
        if len(line) < 3 or line[0] != " ":
            continue
        op, addr = line[1], line[3:].split(",", 1)[0]
        if op == "L":
            out.write(f"R {addr}\n")
        elif op == "S":
            out.write(f"W {addr}\n")
        elif op == "M":
            out.write(f"R {addr}\nW {addr}\n")


if __name__ == "__main__":
    main()
