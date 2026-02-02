#include "engine/engine.hpp"
#include <iostream>

int main() {
    // Queen endgame test
    const std::string fen = "K7/8/7Q/8/8/8/6k1/8 w - - 0 1";
    engine::Engine e(fen);
    e.depth = 10;
    
    std::cout << "Initial FEN: " << fen << "\n";
    std::cout << "Initial eval: " << e.evaluate(e.board) << "\n";
    
    for (int i = 0; i < 10; i++) {
        auto moves = e.generateLegalMoves(e.board);
        if (moves.is_empty()) {
            std::cout << "No legal moves at move " << i << "\n";
            break;
        }
        
        bool whiteToMove = (e.board.getActiveColor() == chess::Board::WHITE);
        auto bestMove = e.getBestMove(moves, whiteToMove);
        
        std::cout << "Move " << i << ": " << chess::Coords::toAlgebric(bestMove.from) 
                  << chess::Coords::toAlgebric(bestMove.to) 
                  << " eval=" << e.eval << "\n";
        
        chess::Board::MoveState state;
        e.board.doMove(bestMove, state);
        
        e.updateGameResult();
        if (e.isMate()) {
            std::cout << "MATE found at move " << i << "!\n";
            return 0;
        }
        
        if (e.board.isStalemate(e.board.getActiveColor())) {
            std::cout << "STALEMATE at move " << i << "!\n";
            return 1;
        }
    }
    
    std::cout << "No mate found in 10 moves\n";
    return 1;
}
