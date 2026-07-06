#pragma once

namespace NNUE {

// ./chess nnue-selftest <net.bin> [games]
// Verifies on random self-play walks that the incrementally-maintained
// accumulator stays byte-identical to a from-scratch rebuild (doMove and
// doMove+undoMove), and prints the reference FEN evals to cross-check
// against nnue/trainer's sanity reader. Exit code 0 = all good.
int runSelfTest(int argc, char* argv[]);

} // namespace NNUE
