#include "../engine.hpp"
#include "../../tests/ut.hpp"
#include <random>
#include <vector>

namespace ut = boost::ut;

ut::suite mateTest = [] {
  using namespace ut;

  "1. mate in 1"_test = []{
    const std::string FEN = "rnbqkb1r/ppp1pp1p/6p1/4N3/2B5/2n5/PPPP1PPP/R1BQK2R w KQkq - 0 7";
    const chess::Board::Move EXPECTED_MOVE{chess::Coords("c4"), chess::Coords("f7")};
    bool whiteToMove = true;

    engine::Engine e(FEN);
    e.depth = 4;

    auto moves = e.generateLegalMoves(e.board);
    chess::Board::Move bestMove = e.getBestMove(moves, whiteToMove);

    expect(bestMove.from == EXPECTED_MOVE.from && bestMove.to == EXPECTED_MOVE.to)
      << "Expected mate move " << EXPECTED_MOVE.from.toString() << EXPECTED_MOVE.to.toString()
      << ", but got " << bestMove.from.toString() << bestMove.to.toString() << '\n';
  };
  
  "2. mate in 1"_test = []{
    const std::string FEN = "8/pp1Rrkp1/2p4p/5K2/2P1r3/1P2N1P1/P6P/3R4 b - - 0 1";
    const chess::Board::Move EXPECTED_MOVE{chess::Coords("g7"), chess::Coords("g6")};
    bool whiteToMove = false;

    engine::Engine e(FEN);
    e.depth = 4;

    auto moves = e.generateLegalMoves(e.board);
    chess::Board::Move bestMove = e.getBestMove(moves, whiteToMove);

    expect(bestMove.from == EXPECTED_MOVE.from && bestMove.to == EXPECTED_MOVE.to)
      << "Expected mate move " << EXPECTED_MOVE.from.toString() << EXPECTED_MOVE.to.toString()
      << ", but got " << bestMove.from.toString() << bestMove.to.toString() << '\n';
  };

  "3. mate in 1"_test = []{
    const std::string FEN = "2kr1bnr/1p1nq1p1/B1p3b1/3p2p1/Q2PpBP1/2P3N1/PP3P1P/R3K2R w - - 0 1";
    const chess::Board::Move EXPECTED_MOVE{chess::Coords("a4"), chess::Coords("c6")};
    bool whiteToMove = true;

    engine::Engine e(FEN);
    e.depth = 4;

    auto moves = e.generateLegalMoves(e.board);
    chess::Board::Move bestMove = e.getBestMove(moves, whiteToMove);

    expect(bestMove.from == EXPECTED_MOVE.from && bestMove.to == EXPECTED_MOVE.to)
      << "Expected mate move " << EXPECTED_MOVE.from.toString() << EXPECTED_MOVE.to.toString()
      << ", but got " << bestMove.from.toString() << bestMove.to.toString() << '\n';
  };

  // ========== MATE IN 2 TESTS ==========

  "1. mate in 2"_test = []{
    const std::string FEN = "3r2k1/1qp3p1/pp6/2p1P1N1/P2n2P1/1P1Qpr2/2P3K1/7R w - - 0 1";

    const chess::Board::Move EXPECTED_FIRST_MOVE{chess::Coords("h1"), chess::Coords("h8")};

    std::vector<chess::Board::Move> possibleResponses = {
      {chess::Coords("g8"), chess::Coords("h8")}
    };

    bool whiteToMove = true;

    engine::Engine e(FEN);
    e.depth = 6;

    auto whiteMoves = e.generateLegalMoves(e.board);
    chess::Board::Move firstMove = e.getBestMove(whiteMoves, whiteToMove);

    expect(firstMove.from == EXPECTED_FIRST_MOVE.from && firstMove.to == EXPECTED_FIRST_MOVE.to)
      << "Expected first move " << EXPECTED_FIRST_MOVE.from.toString() << EXPECTED_FIRST_MOVE.to.toString()
      << ", but got " << firstMove.from.toString() << firstMove.to.toString() << '\n';

    chess::Board::MoveState state1;
    e.board.doMove(firstMove, state1);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, possibleResponses.size() - 1);
    chess::Board::Move opponentResponse = possibleResponses[dis(gen)];

    chess::Board::MoveState state2;
    e.board.doMove(opponentResponse, state2);

    auto finalMoves = e.generateLegalMoves(e.board);
    chess::Board::Move matingMove = e.getBestMove(finalMoves, whiteToMove);

    chess::Board::MoveState state3;
    e.board.doMove(matingMove, state3);

    e.setGameResult();
    bool isMate = e.isMate();

    expect(isMate) << "Expected checkmate but position is not mate\n";
  };

  "2. mate in 2"_test = []{
    const std::string FEN = "2kr3r/p4p2/4b2p/3pq3/Q4p2/2P5/P3B1PP/1R4K1 w - - 0 1";

    const chess::Board::Move EXPECTED_FIRST_MOVE{chess::Coords("a4"), chess::Coords("c6")};

    std::vector<chess::Board::Move> possibleResponses = {
      {chess::Coords("e5"), chess::Coords("c7")}
    };

    bool whiteToMove = true;

    engine::Engine e(FEN);
    e.depth = 6;

    auto whiteMoves = e.generateLegalMoves(e.board);
    chess::Board::Move firstMove = e.getBestMove(whiteMoves, whiteToMove);

    expect(firstMove.from == EXPECTED_FIRST_MOVE.from && firstMove.to == EXPECTED_FIRST_MOVE.to)
      << "Expected first move " << EXPECTED_FIRST_MOVE.from.toString() << EXPECTED_FIRST_MOVE.to.toString()
      << ", but got " << firstMove.from.toString() << firstMove.to.toString() << '\n';

    chess::Board::MoveState state1;
    e.board.doMove(firstMove, state1);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, possibleResponses.size() - 1);
    chess::Board::Move opponentResponse = possibleResponses[dis(gen)];

    chess::Board::MoveState state2;
    e.board.doMove(opponentResponse, state2);

    auto finalMoves = e.generateLegalMoves(e.board);
    chess::Board::Move matingMove = e.getBestMove(finalMoves, whiteToMove);

    chess::Board::MoveState state3;
    e.board.doMove(matingMove, state3);

    e.setGameResult();
    bool isMate = e.isMate();

    expect(isMate) << "Expected checkmate but position is not mate\n";
  };

};
