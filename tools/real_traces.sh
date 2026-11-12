#!/usr/bin/env bash
# Capture data-access traces of real programs with Valgrind Lackey, then run
# cachesim on each. Needs valgrind. Run from the repo root:
#   tools/real_traces.sh      # traces in build/traces, Markdown table on stdout
# Traces include program start-up (loader, libc): about 2.6 M accesses.
set -euo pipefail

T=build/traces
NAMES=(gzip bzip2 xz sort grep python)
mkdir -p "$T"
make -s build/cachesim

# Inputs: 1 MB of C headers and 200,000 random integers from a fixed seed.
find /usr/include -name '*.h' | LC_ALL=C sort | xargs cat 2>/dev/null | head -c 1000000 > "$T/text.in" || true
shuf -i 1-10000000 -n 200000 --random-source=<(yes) > "$T/nums.in"

# Sets CMD to the argv of workload $1.
command_for() {
  case $1 in
    gzip) CMD=(gzip -6 -c "$T/text.in") ;;
    bzip2) CMD=(bzip2 -9 -c "$T/text.in") ;;
    xz) CMD=(xz -1 -c "$T/text.in") ;;
    sort) CMD=(sort -n "$T/nums.in") ;;
    grep) CMD=(grep -cE '[a-z]+_t \*?[a-z_]+\(' "$T/text.in") ;;
    python) CMD=(python3 -c 'd = {i * 7919 % 100003: i for i in range(200000)}; print(sum(sorted(d)))') ;;
  esac
}

capture() {
  [ -s "$T/$1.trace" ] && return
  local CMD
  command_for "$1"
  # Lackey writes the trace to fd 3; the program's own output is discarded.
  LC_ALL=C valgrind --tool=lackey --trace-mem=yes --log-fd=3 "${CMD[@]}" 3>&1 >/dev/null 2>/dev/null \
    | python3 tools/lackey2trace.py > "$T/$1.trace.part"
  mv "$T/$1.trace.part" "$T/$1.trace"
}

# Three captures at a time: Valgrind is single-threaded and the host is shared.
for name in "${NAMES[@]}"; do
  capture "$name" &
  while [ "$(jobs -rp | wc -l)" -ge 3 ]; do wait -n; done
done
wait

CFG="--size 32K --ways 8 --line 64 --l2-size 256K --l2-ways 8"
field() { awk -v k="$1" -v c="$2" '$1 == k { print $c }'; }
echo "| Program | Accesses | L1 miss | L2 miss | AMAT LRU | SRRIP | BRRIP | LRU + prefetch 4 |"
echo "|---|---:|---:|---:|---:|---:|---:|---:|"
for name in "${NAMES[@]}"; do
  lru=$(./build/cachesim $CFG "$T/$name.trace")
  printf '| %s | %s | %s | %s | %s | %s | %s | %s |\n' "$name" \
    "$(field L1 2 <<<"$lru")" "$(field L1 4 <<<"$lru")" "$(field L2 4 <<<"$lru")" "$(field AMAT 2 <<<"$lru")" \
    "$(./build/cachesim $CFG --policy srrip "$T/$name.trace" | field AMAT 2)" \
    "$(./build/cachesim $CFG --policy brrip "$T/$name.trace" | field AMAT 2)" \
    "$(./build/cachesim $CFG --prefetch 4 "$T/$name.trace" | field AMAT 2)"
done
