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
    // Test funzionale: verifica che nelle prime 10 mosse di una partita bot vs bot
    // il delta material rimanga tra -3 e +3 (evita errori grossolani di valutazione)
    
    engine::Engine engine;
    engine.depth = 8; // Profondità moderata per velocità del test
    
    constexpr int MAX_MOVES = 10;
    constexpr int64_t MAX_DELTA = 300;
    bool error = false;
    std::string moves = "";
    int64_t materialDelta = 0;

    for (int moveNum = 1; moveNum <= MAX_MOVES; ++moveNum) {
      // Controlla che il gioco non sia terminato
      if (engine.isMate()) {
        break;
      }
      
      // Genera mosse legali per il turno corrente
      auto legalMoves = engine.generateLegalMoves(engine.board);
      
      // Se non ci sono mosse legali, esci
      if (legalMoves.size == 0) {
        break;
      }
      
      // Determina quale giocatore deve muovere (WHITE=0x0, BLACK=0x8)
      bool isWhiteTurn = (engine.board.getActiveColor() == chess::Board::WHITE);
      
      // Trova la mossa migliore tramite search
      engine.search(engine.depth);
      auto bestMove = engine.getBestMove(legalMoves, isWhiteTurn);
      
      // Esegui la mossa
      (void)engine.board.moveBB(bestMove.from, bestMove.to);
      moves += std::to_string(moveNum) + bestMove.from.toString() + bestMove.to.toString() + "\n";
      
      // Calcola il delta material dopo la mossa
      materialDelta = engine::Engine::getMaterialDelta(engine.board);
      
      // Verifica che il delta sia nell'intervallo accettabile
      if(std::abs(materialDelta) <= MAX_DELTA){
        error = true;
        break;
      }
    }
    expect(error) 
      << "Problema con mosse: " << moves 
      << "In questo momento siamo a delta: " << materialDelta;
  };

};

