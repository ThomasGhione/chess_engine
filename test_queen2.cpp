#include "engine/engine.hpp"
#include <iostream>

int main() {
    const std::string fen = "K7/8/7Q/8/8/8/6k1/8 w - - 0 1";
    engine::Engine e(fen);
    e.depth = 5;  // Lower depth for debugging
    
    std::cout << "Testing Queen endgame\n";
    std::cout << "FEN: " << fen << "\n\n";
    
    auto moves = e.generateLegalMoves(e.board);
    std::cout << "Legal moves: " << moves.size << "\n";
    
    // Test first move
    chess::Board::Move firstMove = moves[0];
    std::cout << "Testing move: " << chess::Coords::toAlgebric(firstMove.from) 
              << chess::Coords::toAlgebric(firstMove.to) << "\n";
    
    chess::Board::MoveState state;
    e.board.doMove(firstMove, state);
    
    std::cout << "After move eval: " << e.evaluate(e.board) << "\n";
    std::cout << "Black in check: " << (e.board.inCheck(chess::Board::BLACK) ? "YES" : "NO") << "\n";
    std::cout << "Black has legal moves: " << e.generateLegalMoves(e.board).size << "\n";
    std::cout << "Is stalemate: " << (e.board.isStalemate(chess::Board::BLACK) ? "YES" : "NO") << "\n";
    
    return 0;
}
