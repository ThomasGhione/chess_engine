#include "engine/engine.hpp"
#include <iostream>

int main() {
    const std::string fen = "6k1/1pp2pp1/3p2p1/p5K1/r7/8/8/8 b - - 1 34";
    engine::Engine e(fen);
    e.depth = 6;
    
    std::cout << "Critical position 7 test (Black to move, should find mate)\n";
    std::cout << "FEN: " << fen << "\n";
    std::cout << "Initial eval: " << e.evaluate(e.board) << "\n\n";
    
    for (int i = 0; i < 15; i++) {
        auto moves = e.generateLegalMoves(e.board);
        if (moves.is_empty()) {
            std::cout << "No legal moves at move " << i << "\n";
            e.updateGameResult();
            std::cout << "Is mate: " << (e.isMate() ? "YES" : "NO") << "\n";
            break;
        }
        
        bool whiteToMove = (e.board.getActiveColor() == chess::Board::WHITE);
        auto bestMove = e.getBestMove(moves, whiteToMove);
        
        std::cout << "Move " << i << " (" << (whiteToMove ? "White" : "Black") << "): " 
                  << chess::Coords::toAlgebric(bestMove.from) 
                  << chess::Coords::toAlgebric(bestMove.to) 
                  << " eval=" << e.eval << "\n";
        
        chess::Board::MoveState state;
        e.board.doMove(bestMove, state);
        
        uint8_t currentColor = e.board.getActiveColor();
        if (e.board.isStalemate(currentColor)) {
            std::cout << "STALEMATE at move " << i << "!\n";
            std::cout << "Material delta: " << e.getMaterialDelta(e.board) << "\n";
            return 1;
        }
        
        e.updateGameResult();
        if (e.isMate()) {
            std::cout << "MATE found at move " << i << "!\n";
            return 0;
        }
    }
    
    std::cout << "\nNo mate found in 15 moves\n";
    return 1;
}
