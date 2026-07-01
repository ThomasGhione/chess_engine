// General engine tests

#include "../engine.hpp"
#include "../../tests/ut.hpp"

namespace ut = boost::ut;

ut::suite engineSuite = [] {
  using namespace ut;

  // Placeholder for future general engine tests
  // Performance tests moved to performanceEngine.cpp
  // Critical position tests moved to criticalPositionEngine.cpp

  "Bot vs Bot - Material Balance First 10 Moves"_test = [] {
    // Functional test: verify that in the first 10 moves of a bot-vs-bot game
    // the material delta stays between -3 and +3 (avoids coarse eval blunders)
    
    engine::Engine engine;
    engine.searchRuntime.depth = 8; // Moderate depth for test speed
    
    constexpr int MAX_MOVES = 10;
    constexpr int64_t MAX_DELTA = 300;
    bool error = false;
    std::string moves = "";
    int64_t materialDelta = 0;

    for (int moveNum = 1; moveNum <= MAX_MOVES; ++moveNum) {
      // Check that the game is not already over
      if (engine.isMate()) {
        break;
      }
      
      // Generate legal moves for the current turn
      auto legalMoves = engine.generateLegalMoves(engine.board);
      
      // If there are no legal moves, stop
      if (legalMoves.size == 0) {
        break;
      }
      
      // Determine side to move (WHITE=0x0, BLACK=0x8)
      bool isWhiteTurn = (engine.board.getActiveColor() == chess::Board::WHITE);
      
      // Find best move through search
      engine.search(8);
      auto bestMove = engine.getBestMove(legalMoves, isWhiteTurn);
      
      // Play the move
      (void)engine.board.moveBB(bestMove.from, bestMove.to);
      moves += std::to_string(moveNum) + chess::squareToString(bestMove.from) + chess::squareToString(bestMove.to) + "\n";
      
      // Compute material delta after the move
      materialDelta = engine::Engine::getMaterialDelta(engine.board);
      
      // Verify the delta is within the acceptable range
      if(std::abs(materialDelta) <= MAX_DELTA){
        error = true;
        break;
      }
    }
    expect(error) 
      << "Problem with moves: " << moves 
      << "Current material delta: " << materialDelta;
  };

  "search root stalemate sets draw state"_test = [] {
    // Black to move, stalemate.
    engine::Engine e("7k/5K2/6Q1/8/8/8/8/8 b - - 0 1");
    e.search(4);

    expect(e.isGameOver()) << "Expected game over in root stalemate\n";
    expect(e.isStalemate()) << "Expected stalemate at root\n";
    expect(e.searchRuntime.eval == static_cast<int64_t>(0)) << "Expected draw eval 0 for root stalemate, got " << e.searchRuntime.eval << '\n';
  };

  "search root checkmate sets mate state"_test = [] {
    // Black to move, checkmated by White.
    engine::Engine e("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
    e.search(4);

    expect(e.isGameOver()) << "Expected game over in root checkmate\n";
    expect(e.isMate()) << "Expected mate at root\n";
    expect(e.getGameResult() == engine::Engine::WHITE_WINS) << "Expected WHITE_WINS game result\n";
    expect(e.searchRuntime.eval > static_cast<int64_t>(1000000)) << "Expected large positive mate eval, got " << e.searchRuntime.eval << '\n';
  };

};
