#pragma once

// Self-play training-data generation (NNUE_PLAN.md, Fase 0/1).
// Both entry points are dispatched from main() before Engine is constructed.

namespace NNUE {

// ./chess datagen [outPrefix] [threads] [nodesPerMove] [netPath] [tbPath]
//   defaults: nnue/data/hydray  3  8000  <embedded net>  engine/syzygy/files
// Positions are labeled by NNUE search with the given (or embedded) network
// (reinforcement loop: regenerate with the strongest net available). With
// tablebases present, games are adjudicated exactly on entering TB range;
// missing tables just disable adjudication.
// Each thread appends bulletformat records to <outPrefix>.t<N>.bin (append =
// safe to stop with Ctrl+C / SIGTERM and resume later).
int runDatagen(int argc, char* argv[]);

// ./chess datagen-dump <file.bin> [count]
// Prints record count and the first `count` positions as FEN|score|result
// for eyeball verification of the binary format.
int runDatagenDump(int argc, char* argv[]);

} // namespace NNUE
