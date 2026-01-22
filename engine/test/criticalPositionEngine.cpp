// Critical position tests for the chess engine

#include "../engine.hpp"
#include "../../tests/ut.hpp"

namespace ut = boost::ut;

ut::suite criticalPositionEngineSuite = [] {
  using namespace ut;

  "is engine generating & sorting castling rights correctly"_test = []{
    engine::Engine e = engine::Engine("8/6pp/6p1/6pk/2p1p1p1/1pPpPpPp/1P1P1P1P/3NK2R w K - 0 1");
    e.depth = 3;

    auto moves = e.generateLegalMoves(e.board);
    expect(moves.size == 4) 
      << "Expected 4 legal moves, got " << moves.size << '\n';  // Only king-side castling and king moves are legal


    int castleMoveIndex = -1;

    castleMoveIndex = std::distance(moves.begin(), 
      std::find_if(moves.begin(), moves.end(), [](const chess::Board::Move& m) {
        return (m.from == chess::Coords("e1") && m.to == chess::Coords("g1"));
      })
    );

    auto sortedMoves = e.sortLegalMoves(moves, 0, e.board, true, 0);
    expect(sortedMoves[0].move.from == chess::Coords("e1") && sortedMoves[0].move.to == chess::Coords("g1"))
      << "Expected castling move to be prioritized, but got move from " << sortedMoves[0].move.from.toString() << sortedMoves[0].move.to.toString() << '\n'
      << "with score " << sortedMoves[0].score << '\n'
      << "Castling move at index:" << castleMoveIndex << '\n';
  };

  "is engine generating pin correctly"_test = []{
    engine::Engine e = engine::Engine("8/r5p1/5npk/6p1/6P1/6PR/6PK/8 b - - 0 1");
    e.depth = 2;

    auto moves = e.generateLegalMoves(e.board);

    expect(moves.size == 1) // Only knight move is legal
      << "Expected 1 legal move, got " << moves.size << '\n'
      << moves[0].from.toString() << moves[0].to.toString() << '\n'
      << moves[1].from.toString() << moves[1].to.toString() << '\n';
  };

  "critical position 1"_test = []{
    engine::Engine e = engine::Engine("r3kbnr/pppbpppp/4q3/8/1n6/P1NPB3/1PP1NPPP/R2QKB1R b KQkq - 0 1");
    e.depth = 6;

    auto moves = e.generateLegalMoves(e.board);

    int64_t evaluation = e.evaluate(e.board);

    chess::Board::Move bestMove = e.getBestMove(moves, false);

    expect(moves.size == 46)
      << "Expected 46 legal moves, got " << moves.size << '\n';

    if(bestMove.from == chess::Coords("b4")){
      expect(bestMove.to != chess::Coords("c2"))
        << "Expected not sacrifice the knight.";
    }
  };

  "critical position 2"_test = []{
    engine::Engine e = engine::Engine("r3kbnr/pppbpppp/4q3/8/8/P1NPB3/1Pn1NPPP/R2QKB1R w KQkq - 0 1");
    e.depth = 6;

    auto moves = e.generateLegalMoves(e.board);

    expect(moves.size == 2) // Expected number of legal moves
      << "Expected 2 legal moves, got " << moves.size << '\n';
  };
}; // ut::suite criticalPositionEngineSuite
