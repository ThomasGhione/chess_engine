#!/usr/bin/env bash
# Build + run the deterministic single-thread node-count bench.
#
# Self-contained: compiles every engine module into output_nb/ with consistent
# prod-like flags (NO -DDEBUG, so the stale debug.hpp trace path is never built,
# and no ODR mismatch with the project's -DDEBUG output/ tree). Node counts at a
# fixed depth are deterministic single-threaded and independent of opt flags, so
# this is a fast, reliable behavior-preservation signal for search refactors.
#
#   Baseline @ depth 11 == 4002856 total nodes; MUST stay identical.
#
# Usage: tuning/nodebench.sh [depth]
set -euo pipefail
cd "$(dirname "$0")/.."

DEPTH="${1:-11}"
OBJ_DIR="output_nb"
CXXFLAGS="-std=c++23 -fopenmp -march=native -O3 -funroll-loops"
CFLAGS="-std=c11 -march=native -O3"

# All C++ module sources: engine/, board/, uci/, driver/ minus test dirs & mains.
mapfile -t CPP_SRCS < <(find engine board uci driver -name '*.cpp' \
    ! -path '*/test/*' ! -name 'mainTest.cpp' ! -name 'mainPerformanceTest.cpp')
C_SRCS=(engine/syzygy/tbprobe.c)

OBJS=()
for src in "${CPP_SRCS[@]}"; do
    obj="$OBJ_DIR/${src%.cpp}.o"; OBJS+=("$obj")
    if [ "$src" -nt "$obj" ] || [ ! -f "$obj" ]; then
        mkdir -p "$(dirname "$obj")"
        g++ $CXXFLAGS -c "$src" -o "$obj"
    fi
done
for src in "${C_SRCS[@]}"; do
    obj="$OBJ_DIR/${src%.c}.o"; OBJS+=("$obj")
    if [ "$src" -nt "$obj" ] || [ ! -f "$obj" ]; then
        mkdir -p "$(dirname "$obj")"
        gcc $CFLAGS -c "$src" -o "$obj"
    fi
done

g++ $CXXFLAGS tests/nodebench.cpp "${OBJS[@]}" -o tests/nodebench
./tests/nodebench "$DEPTH"
