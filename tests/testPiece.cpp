//TODO make tests for other pieces.
// Now a moment I have done Pawn.


#include "ut.hpp"

namespace ut = boost::ut;

ut::suite piceSuite = [] {
  using namespace ut;

  "Pawn"_test = []{
    // In posizione con FEN:
    // 2kr1b1r/ppp2ppp/2n5/4Pb2/8/4BN2/PPP2PPP/R1K2B1R b - - 2 10
    
    // Controllo che pedoni:
    // a2 b2 c2 e5 g2 h2 a7 b7 f7 g7 h7
    // Possano avanzare di 1 casella

    // Controllo che pedoni:
    // a2 b2 c2 g2 h2 a7 b7 g7 h7
    // Possano avanzare di due casella

    // Controlla che pedoni:
    // f2 c7
    // Non possano avanzare
  };
  "Pawn"_test = []{
    // In posizione con FEN:
    // r1bq1rk1/1pp2ppp/p1p2n2/2b5/3NP3/2P2P2/PP4PP/RNBQ1RK1 b - - 0 9
    
    // Controllo che pedoni:
    // a6 b7 g7 h7 a2 b2 c3 e4 f3 g2 h2
    // Possano avanzare di 1 casella

    // Controllo che pedoni:
    // a2 b2 g2 h2 b7 g7 h7
    // Possano avanzare di due casella

    // Controlla che pedoni:
    // c6 c7
    // Non possano avanzare
  };
  "Pawn"_test = []{
    // In posizione con FEN:
    // rnbq1rk1/pp1p1ppp/2p1pn2/8/1bPPP3/2N2N2/PPQ2PPP/R1B1KB1R b KQ - 0 6
    
    // Controllo che pedoni:
    // a2 b2 c4 d4 e4 g2 h2 a7 b7 c6 d7 e6 g7 h7
    // Possano avanzare di 1 casella

    // Controllo che pedoni:
    // a2 g2 h2 a7 b7 d7 g7 h7
    // Possano avanzare di due casella

    // Controlla che pedoni:
    // f2 f7
    // Non possano avanzare
  };
  "Pawn"_test = []{
    // In posizione con FEN:
    // 2kr2nr/ppp1qppp/2nb4/5b2/3PP3/2N2N2/PPP3PP/R1BQKB1R w KQ - 1 8
    
    // Controllo che pedoni:
    // a2 b2 d4 e4 g2 h2 a7 b7 f7 g7 h7
    // Possano avanzare di 1 casella

    // Controllo che pedoni:
    // a2 b2 g2 h2 a7 b7 g7 h7
    // Possano avanzare di due casella

    // Controlla che pedoni:
    // c2 c7
    // Non possano avanzare
  };

  "Rook"_test = []{
    // In posizione con fen:
    // 2kr2nr/ppp1qppp/2nb4/5b2/3PP3/2N2N2/PPP3PP/R1BQKB1R w KQ - 1 8

    // Controllare che possa raggiungere queste caselle:
    // b1 g1 d7 e8 f8
  };
  "Rook"_test = []{
    // In posizione con fen:
    // 1nbqkbn1/1pppppp1/r7/p4R1p/P3r2P/R7/1PPPPPP1/1NBQKBN1 w - - 8 7

    // Controllare che possa raggiungere queste caselle:
    // a1 a2 b3 c3 d3 e3 f3 g3 h3
    // a7 a8 b6 c6 d6 e6 f6 g6 h6
    // a4 b4 c4 d4 f4 g4 h4
    // a5 b5 c5 d5 e5 g5 h5
  };

  "Knight"_test = []{
    // In posizione con fen:
    // r3k2r/ppqn1p1p/2p1pnpP/2PpN3/3P4/2NQP3/PP3PP1/R3K2R w KQkq - 2 15

    // Controllare che cavalli possano raggiungere:
    // Da c3: b1 d1 e2 e4 d5 b5 a4
    // Da e5: f3 g4 g6 f7 d7 c6 c4
    // Da d7: b8 b6 c5 e5 f8
    // Da f6: e4 g4 h5 g8
  };
  
  "Knight"_test = []{
    // In posizione con fen:
    // r3k2r/pp1nbpp1/1qp1p2p/3p1b2/2PPnB2/1QN2NP1/PP2PPBP/R3R1K1 w kq - 2 11

    // Controllare che cavalli possano raggiungere:
    // Da c3: b1 d1 e4 d5 b5 a4
    // Da f3: d2 e5 g5 h4
    // Da e4: c3 d2 f2 g3 g5 f6 d6 c5
    // Da d7: b8 c5 e5 f6 f8
  };
};
