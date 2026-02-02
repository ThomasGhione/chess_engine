#include "board/board.hpp"
#include "engine/engine.hpp"
#include <iostream>

void testPosition(const std::string& fen, const std::string& description) {
    chess::Board board(fen);
    engine::Engine eng;
    
    int64_t fullEval = eng.evaluate(board);
    int64_t queenPressure = eng.evalQueenEndgamePressure(board);
    
    // Get black king position
    uint64_t blackKingBB = board.kings_bb[1];
    int blackKingSq = __builtin_ctzll(blackKingBB);
    int rank = chess::Board::rankOf(blackKingSq);
    int file = chess::Board::fileOf(blackKingSq);
    int distToEdge = std::min({rank, 7 - rank, file, 7 - file});
    
    std::cout << description << "\n";
    std::cout << "  FEN: " << fen << "\n";
    std::cout << "  Black King: sq=" << blackKingSq << " file=" << file << " rank=" << rank << " distToEdge=" << distToEdge << "\n";
    std::cout << "  Full eval: " << fullEval << " cp\n";
    std::cout << "  Queen pressure bonus: " << queenPressure << " cp\n\n";
}

int main() {
    std::cout << "=== QUEEN ENDGAME EVALUATION TEST ===\n\n";
    
    // Original test position
    testPosition("K7/8/7Q/8/8/8/6k1/8 w - - 0 1", 
                 "1. Test position (Ka8, Qh6, Kg2)");
    
    // King in center (worst for attacker)
    testPosition("8/8/3Q4/3k4/8/8/8/K7 w - - 0 1", 
                 "2. Black king in center (Kd5) - dist=2");
    
    // King in corner (best for attacker)
    testPosition("7k/6Q1/8/8/8/8/8/K7 w - - 0 1", 
                 "3. Black king in corner (Kh8) - dist=0");
    
    // King on edge
    testPosition("8/7k/6Q1/8/8/8/8/K7 w - - 0 1", 
                 "4. Black king on edge (Kh7) - dist=0");
    
    // Different king positions to see progression
    testPosition("8/8/6Qk/8/8/8/8/K7 w - - 0 1", 
                 "5. Black king h6 (edge) - dist=0");
    
    testPosition("8/8/6Q1/7k/8/8/8/K7 w - - 0 1", 
                 "6. Black king h5 - dist=1");
    
    testPosition("8/8/6Q1/8/7k/8/8/K7 w - - 0 1", 
                 "7. Black king h4 - dist=2");
    
    return 0;
}
