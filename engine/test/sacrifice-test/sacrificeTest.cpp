// Anti-sacrifice regression tests.
//
// Each case is a position where HydraY was observed playing an UNSOUND
// sacrifice in real games (online blitz). The test searches the position to a
// fixed depth and FAILS if the engine still chooses the forbidden move. This
// guards against the eval over-valuing king attacks / initiative relative to
// material (the "too optimistic sacrifice" family of bugs).
//
// To add a case: drop the FEN of the position BEFORE the bad move, plus the
// from/to squares of the move that must NOT be played (coordinate notation).
// Keep the game id + move number in the name so regressions are traceable.

#include <array>

#include "../../engine.hpp"
#include "../../../tests/ut.hpp"

namespace ut = boost::ut;

namespace {

struct SacrificeCase {
  const char* name;
  const char* fen;          // position with the side-to-move about to blunder
  const char* forbiddenFrom; // square the unsound sacrifice moves FROM
  const char* forbiddenTo;   // square the unsound sacrifice moves TO
  int depth;
};

// Positions taken from lost online games (see lichess game ids).
// Size is auto-deduced — just append a row, no count to maintain.
constexpr auto SACRIFICE_CASES = std::to_array<SacrificeCase>({
  // lichess.org/DarpG9pG, move 20: HydraY (Black) played ...Bxf3, an unsound
  // piece sacrifice that lost the game.
  {"DarpG9pG m20 ...Bxf3 (unsound piece sac)",
   "r4rk1/1p2bppp/p3p3/5q2/1P2b3/P1Q1BP2/1P4PP/2R1KB1R b K - 0 20",
   "e4", "f3", 13},

  // lichess.org/ujVRcmY3, move 14: HydraY (White) played Bxg6, an unsound
  // bishop sacrifice that lost the game.
  {"ujVRcmY3 m14 Bxg6 (unsound bishop sac)",
   "r1bqnrk1/1pp1b2p/p3p1p1/3pP2Q/1P6/2PB4/P2N1PPP/R1B2RK1 w - - 0 14",
   "d3", "g6", 13},

  // lichess.org/H8gqsRhX, move 8: HydraY (White) played Bxh7+, an unsound
  // Greek-gift bishop sacrifice that lost the game.
  {"H8gqsRhX m8 Bxh7+ (unsound greek-gift sac)",
   "r1bq1rk1/ppp2ppp/2n1p3/3p3n/1b1P1B2/3BPN2/PPPN1PPP/R2Q1RK1 w - - 6 8",
   "d3", "h7", 13},

  // lichess.org/cKw6haTL, move 12: HydraY (Black) played ...Nxg2, grabbing a
  // pawn next to the king and just losing the knight to Kxg2.
  {"cKw6haTL m12 ...Nxg2 (unsound piece grab)",
   "r2q1rk1/pp1nbpp1/2p1p2p/3pPb2/2PP1n2/5N1P/PP3PP1/RNBQRBK1 b - - 2 12",
   "f4", "g2", 13},

  // lichess.org/cKw6haTL, move 18: HydraY (Black) played ...Bxd6, losing the
  // bishop for a pawn instead of retreating (...Bg5).
  {"cKw6haTL m18 ...Bxd6 (loses bishop for pawn)",
   "r2q1rk1/p2nbpp1/3Pp2p/1pp1Pb2/2P5/5N1P/P2B1PK1/RN1QRB2 b - - 0 18",
   "e7", "d6", 13},
});

} // namespace

ut::suite sacrificeSuite = [] {
  using namespace ut;

  for (const auto& c : SACRIFICE_CASES) {
    test(c.name) = [&c] {
      engine::Engine e;
      e.board = chess::Board(c.fen);
      e.openingEnabled.store(false, std::memory_order_relaxed);
      // Force single-threaded search: with multiple OpenMP threads the move
      // chosen at a fixed depth is non-deterministic (TT update order races),
      // which makes borderline cases flaky across machines/runs.
      // searchRuntime.maxThreads is the value the searcher actually reads; also
      // set requestedThreads so the choice survives any later reset().
      e.requestedThreads = 1;
      e.searchRuntime.maxThreads = 1;

      const chess::Move best = e.searchUCI(engine::time::Limits{.maxDepth = static_cast<int64_t>(c.depth)});
      const bool playsSac = best.from == chess::parseSquare(c.forbiddenFrom)
                         && best.to == chess::parseSquare(c.forbiddenTo);

      expect(!playsSac)
        << "Engine played forbidden sacrifice " << c.forbiddenFrom << c.forbiddenTo
        << " at depth " << c.depth << " [" << c.name << "]. Got "
        << chess::squareToString(best.from) << chess::squareToString(best.to) << '\n';
    };
  }
};
