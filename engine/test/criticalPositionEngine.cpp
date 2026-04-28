// Critical position tests for the chess engine

#include "../engine.hpp"
#include "../../tests/ut.hpp"

namespace ut = boost::ut;

ut::suite criticalPositionEngineSuite = [] {
  using namespace ut;

  "critical position 17, avoid sacrificing knight"_test = []{
    // Position from a real game where the engine sacrificed its knight on f2/g3
    // for a pawn. This is a bad trade: knight (320) for pawn (100).
    // Stockfish recommends g5g4 or other quiet moves.
    engine::Engine e = engine::Engine("kq6/pp3p2/4b2p/1NP1p1p1/2P1n3/Q3P1P1/P1B2PP1/6K1 b - - 8 39");
    e.depth = 10;

    // Use search() for realistic behavior (iterative deepening + move ordering),
    // rather than getBestMove() directly which skips iterative deepening.
    e.search(e.depth);
    chess::Board::Move bestMove = e.bestMove;

    // The knight on e4 should NOT be sacrificed on g3, f2, or c3
    // (capturing defended pawns loses material: knight 320 vs pawn 100).
    const bool isKnightSacrifice = (bestMove.from == chess::Coords("e4")) &&
      (bestMove.to == chess::Coords("g3") || bestMove.to == chess::Coords("f2") || bestMove.to == chess::Coords("c3"));

    // let's keep this as always fail to make sure we see the move that the engine is trying to play, which should be a knight sacrifice if the bug is present
    expect(false && !isKnightSacrifice)
      << "Shouldn't sacrifice the knight, got move " << bestMove.from.toString() << bestMove.to.toString() << '\n';
  };

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

    e.evaluate(e.board);

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

  "critical position 4"_test = [] {
    engine::Engine e = engine::Engine("R7/8/3B2p1/5n2/P2k4/3r3P/5PK1/8 b - - 0 1");
    e.depth = 6;

    auto moves = e.generateLegalMoves(e.board);

    // write a test to see if it generates the illegal move d3-d6:
    bool illegalMoveFound = false;
    chess::Board::Move m;
    for (const auto& move : moves) {
      if (move.from == chess::Coords("d3") && move.to == chess::Coords("d6")) {
        illegalMoveFound = true;
        m = move;
        break;
      }
    }
    expect(!illegalMoveFound) << "Illegal move d3-d6 found in generated moves. move = " << m.from.toString() << m.to.toString() << '\n';
  };

  "critical position 5"_test = []{
    engine::Engine e = engine::Engine("r1bqkb1r/pppppppp/2n2n2/4P3/3P4/8/PPP2PPP/RNBQKBNR b KQkq - 0 3");

    auto moves = e.generateLegalMoves(e.board);

    e.evaluate(e.board);

    chess::Board::Move bestMove = e.getBestMove(moves, false);

    if(bestMove.from == chess::Coords("f6")){
      expect(bestMove.to != chess::Coords("e4"))
        << "Expected not knight in center.";
    }
  };

  "critical position 6"_test = []{
    engine::Engine e = engine::Engine("3r2k1/pp4b1/2p1bn1p/5pp1/2p4B/2N1PN2/PPP2PPP/R4K2 w - - 0 18");

    auto moves = e.generateLegalMoves(e.board);

    e.evaluate(e.board);

    chess::Board::Move bestMove = e.getBestMove(moves, true);

    if(bestMove.from == chess::Coords("h4")){
      expect(bestMove.to != chess::Coords("g5"))
        << "Expected not knight in center.";
    }
  };

  "critical position 7, avoid losing in"_test = []{
    engine::Engine e = engine::Engine("R7/p1p1rkpp/1p6/1P1P2P1/2P2K2/8/8/8 b - - 16 52");

    auto moves = e.generateLegalMoves(e.board);

    e.evaluate(e.board);

    chess::Board::Move bestMove = e.getBestMove(moves, false);

    if(bestMove.from == chess::Coords("a7")){
      expect(bestMove.to != chess::Coords("a5"))
        << "Expected not pawn push.";
    }
  };

  "critical position 8, avoid losing"_test = []{
    engine::Engine e = engine::Engine("2kr3r/1ppqnpp1/p2p1n1p/4p3/2BPP3/P1P2Q1P/2P2PP1/1RBR2K1 b - - 0 15");

    auto moves = e.generateLegalMoves(e.board);

    e.evaluate(e.board);

    chess::Board::Move bestMove = e.getBestMove(moves, false);

    if(bestMove.from == chess::Coords("e5")){
      expect(bestMove.to != chess::Coords("d4"))
        << "Expected not pawn capture.";
    }
  };

  "critical position 9, avoid losing rook"_test = []{
    engine::Engine e = engine::Engine("1k3br1/p3p1pp/b2p1n2/2p5/8/1N2B2P/PPP2PP1/R4RK1 w - - 2 22");

    auto moves = e.generateLegalMoves(e.board);

    e.evaluate(e.board);

    chess::Board::Move bestMove = e.getBestMove(moves, false);

    if(bestMove.from == chess::Coords("b3")){
      expect(bestMove.to != chess::Coords("c5"))
        << "Expected not sacrifice knight.";
    }
  };

  "critical position 10, capture queen instead of bishop"_test = []{
    engine::Engine e = engine::Engine("2kr1r2/ppp1bp1p/3p1Q2/3P4/2N3q1/2N2P2/PP1P2PP/n1BKR3 w - - 1 15");

    auto moves = e.generateLegalMoves(e.board);

    e.evaluate(e.board);

    chess::Board::Move bestMove = e.getBestMove(moves, false);

    if(bestMove.from == chess::Coords("e1")){
      expect(bestMove.to != chess::Coords("e7"))
        << "Expected not sacrifice rook.";
    }
  };

  "critical position 11, avoid losing knight"_test = []{
    engine::Engine e = engine::Engine("r1bqkbnr/ppp1pppp/2n5/3P4/4p3/2N5/PPP2PPP/R1BQKBNR b KQkq - 0 4");

    auto moves = e.generateLegalMoves(e.board);

    e.evaluate(e.board);

    chess::Board::Move bestMove = e.getBestMove(moves, false);

    if(bestMove.from == chess::Coords("g8")){
      expect(bestMove.to != chess::Coords("f6"))
        << "Expected not sacrifice rook.";
    }
  };


  "critical position 12, avoid losing knight"_test = []{
    engine::Engine e = engine::Engine("5rk1/p3pp1p/1rp1b1p1/2n5/8/1PK2NBP/P1P2PP1/4RB1R b - - 6 19");

    auto moves = e.generateLegalMoves(e.board);

    e.evaluate(e.board);

    chess::Board::Move bestMove = e.getBestMove(moves, false);

    if(bestMove.from == chess::Coords("e6")){
      expect(bestMove.to != chess::Coords("b3"))
        << "Expected not sacrifice rook.";
    }
  };

  "critical position 13, avoid losing knight"_test = []{
    engine::Engine e = engine::Engine("3r2k1/1pp1qpp1/5n1p/5R2/r3p1n1/2P3PP/P3Q1B1/RNN4K b - - 0 25");

    auto moves = e.generateLegalMoves(e.board);

    e.evaluate(e.board);

    chess::Board::Move bestMove = e.getBestMove(moves, false);

    if(bestMove.from == chess::Coords("e7")){
      expect(bestMove.to != chess::Coords("d6"))
        << "Expected not sacrifice knight.";
    }
  };

  "critical position 18, avoid Qe3 hanging queen to bishop"_test = []{
    constexpr const char* FEN = "2r1r1k1/1p3pp1/2b1p2p/p2p2b1/Pq1P4/2NQ1N2/RPP2PPP/4R1K1 w - - 4 20";
    engine::Engine e = engine::Engine(FEN);
    e.depth = 10;

    const chess::Board::Move bestMove = e.searchUCI(e.depth);
    const bool playsHangingQueen = bestMove.from == chess::Coords("d3")
      && bestMove.to == chess::Coords("e3");

    expect(!playsHangingQueen)
      << "Critical regression: engine played Qe3, hanging the queen to Bg5xe3. Got "
      << bestMove.from.toString() << bestMove.to.toString() << '\n';
  };

}; // ut::suite criticalPositionEngineSuite
