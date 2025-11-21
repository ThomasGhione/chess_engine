#ifndef ENGINE_HPP
#define ENGINE_HPP

//#include <unordered_map>
#include <cstdint>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <algorithm>

// Usata solo per sleep 
// #include <unistd.h>

#include "../printer/menu.hpp"
#include "../printer/prints.hpp"
#include "../board/board.hpp"
#include "../coords/coords.hpp"

#include "basebonuspenaltyvalues.hpp"
#include "basicrules.hpp"
#include "piecevaluetables.hpp"

namespace engine {

class Engine final {

public:
    Engine();

    chess::Board board;
    bool isPlayerWhite;

    static int64_t globalEval;
    uint64_t depth;

    int64_t eval;

    static uint64_t nodesSearched; 

    //void playGameVsHuman();
    //void playGameVsEngine(bool isWhite); // ciclo prinipale
  

    void search(uint64_t depth);
    int64_t evaluate(const chess::Board& board); 
    
    // TODO It will be in private later when State0 is finished
    bool isMate();


private:

    // Ricerca ricorsiva (alpha-beta) su una posizione
    int64_t searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply);

    // Genera tutte le mosse legali per la posizione corrente di b (nuova/bitboard)
    void generateLegalMoves(const chess::Board& b,
                            std::vector<chess::Board::Move>& moves) const;

    // Versione "vecchia" di move generation, utile per confronti/debug
    void generateLegalMoves_old(const chess::Board& b,
                                std::vector<chess::Board::Move>& moves) const;

    void takePlayerTurn();
    int64_t getMaterialDelta(const chess::Board& b) noexcept;
    int64_t getMaterialDeltaSLOW(const chess::Board& b) noexcept;

    //void takeEngineTurn(); // ...mossa dopo muove l'engine

/*
    int64_t avoidUnfavorableExchanges(int64_t bishopCount, int64_t knightCount, int64_t pawnCount);
    int64_t bonusBishopPair(int64_t bishopCount, int64_t knightCount) noexcept;
*/


    constexpr static int64_t NEG_INF = std::numeric_limits<int64_t>::min();
    constexpr static int64_t POS_INF = std::numeric_limits<int64_t>::max();


    struct ScoredMove {
        chess::Board::Move move;
        int64_t score;
    };

    // Killer moves: up to 2 non-capture moves per ply that previously caused a beta cutoff
    static constexpr int MAX_PLY = 64;
    chess::Board::Move killerMoves[2][MAX_PLY] {};

    // History heuristic: bonus for non-capture moves that often cause cutoffs
    // history[colorIndex][fromIndex][toIndex]
    int history[2][64][64] = {};

    const std::unordered_map<uint8_t, int64_t> pieceValues = {
        {chess::Board::EMPTY, 0},
        {chess::Board::PAWN, PAWN_VALUE},
        {chess::Board::KNIGHT, KNIGHT_VALUE},
        {chess::Board::BISHOP, BISHOP_VALUE},
        {chess::Board::ROOK, ROOK_VALUE},
        {chess::Board::QUEEN, QUEEN_VALUE},
        {chess::Board::KING, KING_VALUE}
    };
};

} 

#endif
