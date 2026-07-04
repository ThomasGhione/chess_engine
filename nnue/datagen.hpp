#pragma once

// Self-play training-data generation (NNUE_PLAN.md, Fase 0/1).
// Both entry points are dispatched from main() before Engine is constructed.

namespace NNUE {

// ./chess datagen [outPrefix] [threads] [nodesPerMove]
//   defaults: nnue/data/hydray  3  8000
// Each thread appends bulletformat records to <outPrefix>.t<N>.bin (append =
// safe to stop with Ctrl+C / SIGTERM and resume later).
int runDatagen(int argc, char* argv[]);

// ./chess datagen-dump <file.bin> [count]
// Prints record count and the first `count` positions as FEN|score|result
// for eyeball verification of the binary format.
int runDatagenDump(int argc, char* argv[]);

} // namespace NNUE
