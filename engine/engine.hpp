#ifndef ENGINE_HPP
#define ENGINE_HPP

//#include <unordered_map>
#include <cstdint>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <cstring>
#include <omp.h>

#ifdef DEBUG
#include <chrono>
#include <iostream>
#endif

// Usata solo per sleep 
// #include <unistd.h>

#include "../printer/menu.hpp"
#include "../printer/prints.hpp"
#include "../board/board.hpp"
#include "../coords/coords.hpp"

#include "basebonuspenaltyvalues.hpp"
#include "basicrules.hpp"
#include "piecevaluetables.hpp"
#include "tt.hpp"
#include "movelist.hpp"

namespace engine {

class Engine final {

public:
    Engine();
    Engine(std::string fen);

    chess::Board board;
    bool isPlayerWhite;

    static int64_t globalEval;
    uint64_t depth;

    int64_t eval = 0;  // Inizializzato a 0 per evitare valori spazzatura

    // Puntatore alla transposition table globale
    TTEntry* ttTable;

    static uint64_t nodesSearched; 

#ifdef DEBUG
    // Transposition table statistics (only in debug builds)
    static uint64_t ttProbes;
    static uint64_t ttHits;
#endif

    void search(uint64_t depth) noexcept;
    int64_t evaluate(const chess::Board& board) noexcept; 
    int64_t evaluateFast(const chess::Board& board, bool isEndgameHint = false) noexcept;
    
    // TODO It will be in private later when State0 is finished
    bool isMate() noexcept;

    int64_t getMaterialDeltaFAST(const chess::Board& b) noexcept;
    int64_t getMaterialDelta(const chess::Board& b) noexcept;

    static constexpr int MAX_PLY = 64;

    struct ScoredMove {
        chess::Board::Move move;
        int64_t score;
    };




    // Genera tutte le mosse legali per la posizione corrente di b (nuova/bitboard)
    MoveList<chess::Board::Move> generateLegalMoves(const chess::Board& b) const noexcept;
    MoveList<ScoredMove> sortLegalMoves(const MoveList<chess::Board::Move>& moves, int ply, chess::Board& b, bool usIsWhite) noexcept;

    chess::Board::Move getBestMove(const MoveList<chess::Board::Move>& moves, bool searchBestMoveForWhite) noexcept;

private:
    // Helper structures to reduce parameter passing
    struct SearchContext {
        int64_t depth;
        int64_t alpha;
        int64_t beta;
        int ply;
        uint8_t activeColor;
    };

    struct AlphaBeta {
        int64_t alpha;
        int64_t beta;
    };

    struct TTSaveInfo {
        uint64_t hashKey;
        int64_t depth;
        int64_t score;
        int64_t alphaOrig;
        int64_t beta;
        uint16_t bestMove; // Add best move to save info
    };

    void doMoveInBoard(chess::Board::Move bestMove) noexcept;
    void updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& bestScore, 
                 chess::Board::Move& bestMove, const chess::Board::Move& m) noexcept;
    void updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& best) noexcept;

    bool shouldPruneLateMove(const chess::Board& b,const chess::Board::Move& m, int64_t depth, bool inCheck, bool usIsWhite, int moveIndex, int totalMoves) noexcept;

    void updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m, int64_t depth, int ply, uint8_t us, int64_t alpha, int64_t beta, int (&history)[2][64][64], chess::Board::Move (&killerMoves)[2][Engine::MAX_PLY]) noexcept;

    // Helper methods for search
    bool handleSearchPrelude(chess::Board& b, int64_t& depth, const AlphaBeta& bounds, int64_t& score) noexcept;
    ScoredMove searchMoves(chess::Board& b, const MoveList<ScoredMove>& orderedScoredMoves,
                          bool usIsWhite, SearchContext& ctx, AlphaBeta& bounds) noexcept;
    bool probeTTCache(uint64_t hashKey, int64_t depth, const AlphaBeta& bounds, int64_t& score) noexcept;
    void saveTTEntry(const TTSaveInfo& info) noexcept;
    bool getHashMove(const chess::Board& b, chess::Board::Move& outMove) noexcept; // Get hash move from TT

    // Helper methods for move execution
    void executeMove(const chess::Board::Move& m, chess::Board::MoveState& state) noexcept;
    void undoAndUpdateMove(const chess::Board::Move& m, chess::Board::MoveState& state, bool usIsWhite,
                          int64_t score, int64_t& alpha, int64_t& beta, int64_t& bestScore,
                          chess::Board::Move& bestMove) noexcept;

    // Helper methods for move scoring
    void addMVVLVABonus(const chess::Board::Move& m, const chess::Board& b, int64_t& score) noexcept;
    void addPromotionBonus(const chess::Board::Move& m, uint8_t pieceType, bool usIsWhite, int64_t& score) noexcept;
    void addCheckBonus(const chess::Board::Move& m, chess::Board& b, bool usIsWhite, int64_t& score) noexcept;
    void addKillerAndHistoryBonus(const chess::Board::Move& m, int ply, bool usIsWhite, int64_t& score) noexcept;
    void addKingMoveBonus(const chess::Board::Move& m, uint8_t pieceType, bool inCheck, int fullMoveClock, int64_t& score) noexcept;
    int64_t staticExchangeEvaluation(const chess::Board& b, const chess::Board::Move& m) const noexcept;

    //void savePositionToTT();
    //bool hasSearchStop(int64_t& depth, chess::Board& b, int64_t& evaluate);
    //MoveList<engine::Engine::ScoredMove> getOrderedScoreMoveForCurrentPosition(chess::Board& b);
    //int64_t cleanSearchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply);
    int64_t searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply) noexcept;
    bool isKillerMove(const chess::Board::Move& m, const chess::Board::Move killerMoves[2][Engine::MAX_PLY], int ply) const noexcept;

    
    int64_t evaluateCheckmate(const chess::Board& board) noexcept;
/*
    int64_t avoidUnfavorableExchanges(int64_t bishopCount, int64_t knightCount, int64_t pawnCount);
    int64_t bonusBishopPair(int64_t bishopCount, int64_t knightCount) noexcept;
*/
    constexpr static int64_t NEG_INF = std::numeric_limits<int64_t>::min();
    constexpr static int64_t POS_INF = std::numeric_limits<int64_t>::max();
    
    // Killer moves: up to 2 non-capture moves per ply that previously caused a beta cutoff
    chess::Board::Move killerMoves[2][MAX_PLY] {};

    // History heuristic: bonus for non-capture moves that often cause cutoffs
    // history[colorIndex][fromIndex][toIndex]
    int history[2][64][64] = {};

    // Lookup array per valori pezzi (più veloce di unordered_map)
    static constexpr int64_t PIECE_VALUES[8] = {
        0,      // EMPTY = 0
        PAWN_VALUE,    // PAWN = 1
        KNIGHT_VALUE,    // KNIGHT = 2
        BISHOP_VALUE,    // BISHOP = 3
        ROOK_VALUE,    // ROOK = 4
        QUEEN_VALUE,    // QUEEN = 5
        KING_VALUE,  // KING = 6
        0       // unused = 7
    };

}; //class Engine final

} // namespace engine

#endif
