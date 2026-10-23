#pragma once

namespace NNUE {

// ./chess nnue-selftest <net.bin> [games] [small.bin]
// Verifies on random self-play walks that the incrementally-maintained
// accumulator stays byte-identical to a from-scratch rebuild (doMove and
// doMove+undoMove), and prints the reference FEN evals to cross-check
// against nnue/trainer's sanity reader. With a small net, checks its
// accumulator the same way and its vectorised forward against the scalar
// one. Exit code 0 = all good.
int runSelfTest(int argc, char* argv[]);

// ./chess small-eval <small.bin> < fens: "small<TAB>big<TAB>fen" per line,
// to diff against sanity_deep.rs and compare the two nets' scales.
int runSmallEval(int argc, char* argv[]);

} // namespace NNUE
