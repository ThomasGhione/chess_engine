## chess_engine ##

# Description
This is a program made in C++ which lets you play chess using the terminal (no graphics libraries).

# Tested systems
- Windows 10-11
- WSL 2
- Linux distros

# Compile
Note: Make is required.

## Windows
First of all, compile the project:
```make prod```

Then, you can run it by:
```a.exe```

Try running "chcp 65001" if chessboard doesn't display properly.
If it doesn't work on Powershell then try it on the CMD.

## Linux
First of all, compile the project:
```make prod```

Then, you can run it by:
```./chess.out```

## MORE INFOS:
Those information is needed only for manual compilation.

Run with -DDEBUG to get the execution time of each move (excluding how much it takes for the user to input the move.
Run with -O3 to get aggressive optimizations, from my tests it looks like it doesn't break the program, and it also
looks like the very first move is gonna take relatively more time but this issue goes away right after.

# How does it work?

The program start with a menu where you can choose what to do.
Once game is started you will have to type:
The coordinates of the square of the piece you want to move and then the coordinates of the destination square.
For example:

Move is pawn to e4, we have to input this: "e2 e4" 
(which can be seen as: "move the piece in e2 to e4)

# Forcing TT huge pages:
- CHESS_TT_HUGEPAGE=on ./chess
- CHESS_TT_HUGEPAGE=off ./chess
- auto otherwise

# Testing TT

1. Build dedicated benchmark:
make tt-huge-bench

2. Run benchmark directly (manual):
./tests/tt_hugepage_bench --depth 10 --repeats 1
./tests/tt_hugepage_bench --depth 10 --repeats 3 --per-fen

3. Run automated A/B OFF vs ON (new script):
./script/benchmark_tt_hugepages_ab.sh 10 1
./script/benchmark_tt_hugepages_ab.sh 10 3
./script/benchmark_tt_hugepages_ab.sh 10 3 output/bench_custom_run

4. Override huge page mode at runtime:
CHESS_TT_HUGEPAGE=off ./tests/tt_hugepage_bench --depth 10 --repeats 1
CHESS_TT_HUGEPAGE=on  ./tests/tt_hugepage_bench --depth 10 --repeats 1
CHESS_TT_HUGEPAGE=auto ./chess

5. (For perf stat metrics) enable counters:
sudo sysctl -w kernel.perf_event_paranoid=-1
cat /proc/sys/kernel/perf_event_paranoid

# Contributing

Pull requests are welcome.
For major changes, please open an issue first to discuss what you would like to change.
