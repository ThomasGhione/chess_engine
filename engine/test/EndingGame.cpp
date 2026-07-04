#include <string>
#include "../engine.hpp"
#include "../../tests/ut.hpp"

namespace ut = boost::ut;

namespace {

  bool findMate(chess::Board board, int maxHalfMoves = 100, int searchDepth = 10) {
    engine::Engine e(board.getCurrentFen());
    e.depth = searchDepth;

    for (int ply = 0; ply < maxHalfMoves; ++ply) {
      e.updateGameResult();
      if (e.isMate()) {
        return true;
      }

      uint8_t currentColor = e.board.getActiveColor();
      if (e.board.isStalemate(currentColor)) {
        printf("Stalemate at half-move %d\n", ply);
        return false;
      }

      auto moves = e.generateLegalMoves(e.board);
      if (moves.size == 0) {
        e.updateGameResult();
        bool isMate = e.isMate();
        if (!isMate) {
          printf("No legal moves but not checkmate at half-move %d\n", ply);
        }
        return isMate;
      }

      bool whiteToMove = (e.board.getActiveColor() == chess::Board::WHITE);
      chess::Move bestMove = e.getBestMove(moves, whiteToMove);

      chess::Board::MoveState state;
      e.board.doMove(bestMove, state);
    }

    printf("Max moves (%d) reached without checkmate\n", maxHalfMoves);
    return false;
  }

  bool isDraw(chess::Board board, int maxHalfMoves = 8, int searchDepth = 10) {
    engine::Engine e(board.getCurrentFen());
    e.depth = searchDepth;

    for (int ply = 0; ply < maxHalfMoves; ++ply) {
      e.updateGameResult();
      if (e.isGameOver()) {
        return e.getGameResult() == engine::Engine::DRAW;
      }

      auto moves = e.generateLegalMoves(e.board);
      if (moves.size == 0) {
        e.updateGameResult();
        return e.getGameResult() == engine::Engine::DRAW;
      }

      bool whiteToMove = (e.board.getActiveColor() == chess::Board::WHITE);
      chess::Move bestMove = e.getBestMove(moves, whiteToMove);

      chess::Board::MoveState state;
      e.board.doMove(bestMove, state);
    }

    e.updateGameResult();
    if (e.isGameOver()) {
      return e.getGameResult() == engine::Engine::DRAW;
    }

    printf("Max moves (%d) reached without game over\n", maxHalfMoves);
    return false;
  }

}

// ==================== TEST SUITE ====================

ut::suite EndingGameSuite = [] {
  using namespace ut;

  "critical position 14, avoid sacrificing knight"_test = []{
    engine::Engine e = engine::Engine("r4rk1/ppp2ppp/2nq4/2R1p3/1P6/P2PB2P/5PP1/3QR1K1 b - - 4 22");

    auto moves = e.generateLegalMoves(e.board);
    e.evaluate(e.board);
    chess::Move bestMove = e.getBestMove(moves, false);

    expect(!(bestMove.from == chess::parseSquare("c6") && bestMove.to == chess::parseSquare("b4")))
      << "Shouldn't sacrifice the knight (c6 b4), got move " << chess::squareToString(bestMove.from) << chess::squareToString(bestMove.to) << '\n';
  };

  "critical position 15, avoid sacrificing queen (after knight)"_test = []{
    engine::Engine e = engine::Engine("r4rk1/ppp2ppp/3q4/2R1p3/1P6/3PB2P/5PP1/3QR1K1 b - - 0 23");

    auto moves = e.generateLegalMoves(e.board);
    e.evaluate(e.board);
    chess::Move bestMove = e.getBestMove(moves, false);

    expect(!(bestMove.from == chess::parseSquare("d6") && bestMove.to == chess::parseSquare("c5")))
      << "Shouldn't sacrifice the queen (d6 c5), got move " << chess::squareToString(bestMove.from) << chess::squareToString(bestMove.to) << '\n';
  };

  "critical position 16, avoid sacrificing queen"_test = [] {
    engine::Engine e = engine::Engine("3r1r1k/2Q3pp/p1p2q1n/1p6/2b1P3/2N1PN1P/PPP2RP1/R5K1 w - - 0 18");
    auto moves = e.generateLegalMoves(e.board);
    e.evaluate(e.board);
    chess::Move bestMove = e.getBestMove(moves, true);
    expect(!(bestMove.from == chess::parseSquare("c7") && bestMove.to == chess::parseSquare("d8")))
      << "Expected not to sacrifice queen.";
  };

  "critical stalemate 2"_test = []{
    const std::string fen = "r3kb1r/ppp2ppp/2n2n2/8/N4P2/2PP4/PP1P1RKP/R1Bq4 b kq - 1 15";
    chess::Board board(fen);
    bool isDrawResult = isDraw(board);
    ut::expect(!isDrawResult)
      << "+11 material for white, shouldn't draw (16 plies).\n";
  };

  "critical stalemate 1"_test = []{
    const std::string fen = "6k1/1pp2pp1/3p2p1/p5K1/r7/8/8/8 b - - 1 34";
    chess::Board board(fen);
    bool foundMate = findMate(board);
    ut::expect(foundMate)
      << "Rook and some pawn: "
      << "Failed to find checkmate within 50 moves (100 plies). [expect mate in 6]\n";
  };

  "Queen endgame"_test = []{
    const std::string fen = "K7/8/7Q/8/8/8/6k1/8 w - - 0 1";
    chess::Board board(fen);
    bool foundMate = findMate(board);
    ut::expect(foundMate)
      << "Queen endgame: "
      << "Failed to find checkmate within 50 moves (100 plies).\n";
  };

  "Rook endgame"_test = []{
    const std::string fen = "8/8/8/8/8/4k3/7R/6K1 w - - 0 1";
    chess::Board board(fen);
    bool foundMate = findMate(board);
    ut::expect(foundMate)
      << "Rook endgame: "
      << "Failed to find checkmate within 50 moves (100 plies).\n";
  };

  "Double rook endgame"_test = []{
    const std::string fen = "8/8/8/8/8/4k3/R6R/6K1 w - - 0 1";
    chess::Board board(fen);
    bool foundMate = findMate(board);
    ut::expect(foundMate)
      << "Double Rook endgame: "
      << "Failed to find checkmate within 50 moves (100 plies).\n";
  };

  "Knight and bishop endgame"_test = []{
    const std::string fen = "8/8/8/8/8/4k3/8/2N1B1K1 w - - 0 1";
    chess::Board board(fen);
    bool foundMate = findMate(board);
    ut::expect(foundMate)
      << "Knight and bishop endgame: "
      << "Failed to find checkmate within 50 moves (100 plies).\n";
  };

  "Double bishop endgame"_test = []{
    const std::string fen = "8/8/8/8/8/4k3/8/4BBK1 w - - 0 1";
    chess::Board board(fen);
    bool foundMate = findMate(board);
    ut::expect(foundMate)
      << "Double bishop endgame: "
      << "Failed to find checkmate within 50 moves (100 plies).\n";
  };

  "Mate in 17 moves"_test = []{
    const std::string fen = "8/8/5k2/4pp2/R5p1/2P1K3/PP4PP/8 b - - 0 37";
    chess::Board board(fen);
    bool foundMate = findMate(board);
    ut::expect(foundMate)
      << "Rook and some pawn: "
      << "Failed to find checkmate within 50 moves (100 plies). [expect mate in 17]\n";
  };

  "Mate in 5 moves"_test = []{
    const std::string fen = "1kbr4/pn3R2/2p3P1/2p1N2p/7P/P1N5/BPP2PP1/2K5 w - - 1 36";
    chess::Board board(fen);
    bool foundMate = findMate(board);
    ut::expect(foundMate)
      << "Rook and Knights: "
      << "Failed to find checkmate within 50 moves (100 plies). [expect mate in 5]\n";
  };

  "Queen vs rook and bishop"_test = []{
    const std::string fen = "8/7r/6bk/4Q3/2P5/2K1P3/PP4P1/8 b - - 8 51";
    chess::Board board(fen);
    bool foundMate = findMate(board);
    ut::expect(foundMate)
      << "Queen ending game: "
      << "Failed to find checkmate within 50 moves (100 plies).\n";
  };

};
