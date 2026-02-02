#include "board/board.hpp"
#include "engine/engine.hpp"
#include <iostream>

int main() {
    // Queen endgame test position: K7/8/7Q/8/8/8/6k1/8 w - - 0 1
    // White King on a8, White Queen on h6, Black King on g2
    
    chess::Board board("K7/8/7Q/8/8/8/6k1/8 w - - 0 1");
    
    std::cout << "Queen endgame test position: K7/8/7Q/8/8/8/6k1/8 w - - 0 1\n";
    std::cout << "(White King a8, White Queen h6, Black King g2)\n\n";
    
    engine::Engine eng;
    int64_t eval = eng.evaluate(board);
    
    std::cout << "\nFull evaluation: " << eval << " cp\n";
    
    // Test just the queen endgame pressure
    int64_t queenPressure = eng.evalQueenEndgamePressure(board);
    std::cout << "Queen endgame pressure bonus: " << queenPressure << " cp\n";
    
    // Check piece counts
    int whiteQueens = __builtin_popcountll(board.queens_bb[0]);
    int blackQueens = __builtin_popcountll(board.queens_bb[1]);
    std::cout << "\nWhite queens: " << whiteQueens << "\n";
    std::cout << "Black queens: " << blackQueens << "\n";
    
    // Black king position (g2 = file 6, rank 6 from top)
    uint64_t blackKingBB = board.kings_bb[1];
    int blackKingSq = __builtin_ctzll(blackKingBB);
    int rank = chess::Board::rankOf(blackKingSq);
    int file = chess::Board::fileOf(blackKingSq);
    std::cout << "\nBlack king at square " << blackKingSq 
              << " (file=" << file << ", rank=" << rank << ")\n";
    
    int distToEdge = std::min({rank, 7 - rank, file, 7 - file});
    std::cout << "Distance to edge: " << distToEdge << "\n";
    std::cout << "Edge proximity (7 - distToEdge): " << (7 - distToEdge) << "\n";
    
    return 0;
}
