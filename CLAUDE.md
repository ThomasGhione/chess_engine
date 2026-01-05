# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a chess engine written in C++23 with a CLI interface. The engine implements an AI capable of playing chess using bitboards, alpha-beta pruning with transposition tables, and various modern optimizations.

**Entry point**: `main.cpp` creates Menu, Engine, and Driver, then calls `driver.startGame()`

## Build & Test Commands

```bash
# Build production binary (parallel compilation)
make prod

# Run the chess program
./chess

# Build with debug symbols
make debug

# Build and compile tests
make test

# Run tests
./tests/outputTest

# Static code analysis
make analyze

# Clean build artifacts
make cls
```

**Note**: The makefile uses parallel compilation by default (`-j$(nproc)`). C++23 is required with OpenMP support.

## Architecture & Key Concepts

### Module Structure

The codebase is organized into self-contained modules:

- **coords/**: Coordinate system using single `uint8_t index` (0-63) internally
- **board/**: Board representation with hybrid array + bitboard approach
- **piece/**: Attack map generation using magic bitboards and lookup tables
- **engine/**: AI search (Negamax + Alpha-Beta) and evaluation
- **driver/**: Game loop orchestration
- **printer/**: CLI interface with Unicode chess symbols
- **gamestatus/**: Game state management

### Critical Design Patterns

#### 1. Board Index Convention (a8=0, h1=63)
The entire codebase uses the convention where:
- **a8 = index 0**, b8 = 1, ..., h8 = 7
- a7 = 8, ..., h1 = 63
- Formula: `index = rank * 8 + file` (where rank 0 = row 8, rank 7 = row 1)

**When working with coords**: Always use `coords.file()` and `coords.rank()` methods (NOT direct member access).

#### 2. Bitboard Representation
Board uses **hybrid representation** for efficiency:
- `std::array<uint32_t, 8> chessboard`: 4 bits per piece (3 bits type + 1 bit color)
- `uint64_t` bitboards for occupancy: fast batch operations and piece-specific bitboards

#### 3. Move/Unmove Pattern
The engine explores the game tree without copying boards:
```cpp
MoveState state = board.movePiece(move);
// ... evaluate position ...
board.unmovePiece(state);  // restore previous state
```

`MoveState` contains all information needed to undo a move (captured pieces, castling rights, en passant, etc.).

#### 4. Attack Generation - Magic Bitboards ✅

**Implementation** (`piece/piece.hpp`, `piece/magic_numbers.hpp`):
- Sliding pieces (Rook, Bishop, Queen) use **magic bitboards**
- Pre-computed lookup tables with perfect hashing
- O(1) attack retrieval without branches (~3-5 CPU cycles)
- **Speedup**: 2.5x for Rook, 4.7x for Bishop, 3.3x average vs previous ray-based approach
- Magic numbers sourced from [magic-bits library](https://github.com/goutham/magic-bits)
- Attack tables: ~800 KB (Rook) + ~40 KB (Bishop)
- Initialized once at engine startup via `pieces::initMagicBitboards()`

### Engine Search Strategy

The AI (`engine::Engine`) implements:
- **Negamax with Alpha-Beta pruning**: Single recursive method instead of separate min/max
- **Transposition Table**: Global hash table with Zobrist hashing to cache evaluated positions
- **Iterative Deepening**: Search depth 1, 2, 3, ... up to target depth
- **Move Ordering**: MVV-LVA for captures, killer moves heuristic, history heuristic
- **Late Move Pruning**: Skip "quiet" moves late in the ordered list at low depths
- **Parallel Search**: OpenMP parallelization at root level

**Key optimization** (see OPTIMIZATIONS.md): `generateLegalMoves()` pre-computes check and pin information once, avoiding expensive `canMoveToBB()` calls for most moves (3-10x speedup).

### Coords Class Refactoring

**Recent change**: `Coords` now stores only a single `uint8_t index_` instead of separate `file` and `rank`. Access via methods:
- `coords.file()`: returns column (0-7)
- `coords.rank()`: returns row (0-7, where 0=row8, 7=row1)
- `coords.index()`: returns internal index (0-63)

These are `constexpr` methods using bit operations (`file = index & 7`, `rank = index >> 3`).

## Important Conventions

### String Notation Conversion
When converting between internal representation and algebraic notation (e.g., "e4"):
- File: `'a' + file` → characters a-h
- Rank: `'8' - rank` → characters 8-1 (inverted because rank 0 = row 8!)

### FEN Support
The Board class supports FEN (Forsyth-Edwards Notation):
- `Board::fromFenToBoard(string)`: load position from FEN
- `Board::toFen()`: export current position to FEN

### Test Framework
Tests use a custom lightweight framework in `tests/ut.hpp`. Each module has its own test directory (e.g., `coords/test/`, `board/test/`).

## Documentation

LaTeX documentation in `doc/`:
```bash
cd doc
pdflatex main-doc.tex
pdflatex main-doc.tex  # run twice for TOC
```

Generated PDF contains detailed architecture, component descriptions, evaluation function details, and optimization explanations.

## Platform-Specific Notes

**Windows**: If Unicode chess symbols don't render, run `chcp 65001` before executing the program. Use CMD if PowerShell has issues.

**Linux/WSL**: Unicode support is typically available by default in modern terminals.

## Recent Improvements

### Magic Bitboards Implementation ✅

**Status**: Completed (2026-01-05)

**Achievement**: Replaced ray-based sliding piece attack generation with magic bitboards.

**Results**:
- **Isolated speedup**: 2.49x (Rook), 4.71x (Bishop), 3.34x average
- **Memory usage**: ~840 KB for lookup tables
- **108,492 validation tests** passed (exhaustive comparison vs previous implementation)

**Implementation**:
- `piece/magic_numbers.hpp`: Magic numbers and masks from [magic-bits library](https://github.com/goutham/magic-bits)
- `piece/piece.hpp`: MagicData struct, initialization, O(1) lookup functions
- Automatic initialization in Engine constructor with thread-safe static flag

**Technical Details**:
- Perfect hashing: `index = ((occupancy & mask) * magic) >> shift`
- Attack tables: 102,400 entries (Rook) + 5,248 entries (Bishop)
- See `magic-bitboards-plan.md` for complete implementation history

---

## Code Style

- Use `this->` for member access in class methods
- Prefer `constexpr` and `noexcept` where applicable for performance
- Bitboard operations: use bit-wise ops (`&`, `|`, `^`, `<<`, `>>`) for speed
- Hardware intrinsics: `__builtin_popcountll()`, `__builtin_ctzll()` for bitboard manipulation
