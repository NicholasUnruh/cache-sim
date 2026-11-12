"""Hand-checked conversion of a Lackey excerpt: the instruction fetch and the
Valgrind banner are dropped, a load becomes R, a store W, a modify R then W."""

import pathlib
import subprocess
import sys

LACKEY = """==600== Lackey, an example Valgrind tool
I  04b66340,3
 S 1fff000958,8
 L 04b9a1c0,4
 M 0025747c,1
==600==
"""

EXPECTED = """W 1fff000958
R 04b9a1c0
R 0025747c
W 0025747c
"""

tool = pathlib.Path(__file__).resolve().parent.parent / "tools" / "lackey2trace.py"
out = subprocess.run([sys.executable, str(tool)], input=LACKEY, capture_output=True, text=True, check=True).stdout
if out != EXPECTED:
    sys.exit(f"FAILED lackey2trace:\n{out}")
print("PASS: lackey2trace")
