#ifndef ENGINE_HPP
#define ENGINE_HPP

//#include <unordered_map>
#include <cstdint>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <algorithm>
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

namespace engine {

class Engine final {

public:
    Engine();

    chess::Board board;
    bool isPlayerWhite;

    static int64_t globalEval;
    uint64_t depth;

    int64_t eval;

    // Puntatore alla transposition table globale
    TTEntry* ttTable;

    static uint64_t nodesSearched; 

#ifdef DEBUG
    // Transposition table statistics (only in debug builds)
    static uint64_t ttProbes;
    static uint64_t ttHits;
    static uint64_t ttExactHits;
    static uint64_t ttCutoffHits;
#endif

    void search(uint64_t depth);
    int64_t evaluate(const chess::Board& board); 
    
    // TODO It will be in private later when State0 is finished
    bool isMate();

    int64_t getMaterialDeltaFAST(const chess::Board& b) noexcept;
    int64_t getMaterialDelta(const chess::Board& b) noexcept;

    static constexpr int MAX_PLY = 64;

    struct ScoredMove {
        chess::Board::Move move;
        int64_t score;
    };
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
    };

    void doMoveInBoard(chess::Board::Move bestMove);
    chess::Board::Move getBestMove(std::vector<chess::Board::Move> moves, bool searchBestMoveForWhite);
    void updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& bestScore, 
                 chess::Board::Move& bestMove, const chess::Board::Move& m);
    void updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& best);

    bool shouldPruneLateMove(const chess::Board& b,const chess::Board::Move& m, int64_t depth, bool inCheck, bool usIsWhite, int moveIndex, int totalMoves);

    void updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m, int64_t depth, int ply, uint8_t us, int64_t alpha, int64_t beta, int (&history)[2][64][64], chess::Board::Move (&killerMoves)[2][Engine::MAX_PLY]);

    // Helper methods for search
    bool handleSearchPrelude(chess::Board& b, int64_t& depth, const AlphaBeta& bounds, int64_t& score);
    ScoredMove searchMoves(chess::Board& b, const std::vector<ScoredMove>& orderedScoredMoves,
                          bool usIsWhite, SearchContext& ctx, AlphaBeta& bounds);
    bool probeTTCache(uint64_t hashKey, int64_t depth, const AlphaBeta& bounds, int64_t& score);
    void saveTTEntry(const TTSaveInfo& info);

    // Helper methods for move execution
    void executeMove(const chess::Board::Move& m, chess::Board::MoveState& state);
    void undoAndUpdateMove(const chess::Board::Move& m, chess::Board::MoveState& state, bool usIsWhite,
                          int64_t score, int64_t& alpha, int64_t& beta, int64_t& bestScore,
                          chess::Board::Move& bestMove);

    // Helper methods for move scoring
    void addMVVLVABonus(const chess::Board::Move& m, const chess::Board& b, int64_t& score);
    void addPromotionBonus(const chess::Board::Move& m, uint8_t pieceType, bool usIsWhite, int64_t& score);
    void addCheckBonus(const chess::Board::Move& m, chess::Board& b, bool usIsWhite, int64_t& score);
    void addKillerAndHistoryBonus(const chess::Board::Move& m, int ply, bool usIsWhite, int64_t& score);
    void addKingMoveBonus(chess::Board& b, const chess::Board::Move& m, uint8_t pieceType, int64_t& score);

    //void savePositionToTT();
    //bool hasSearchStop(int64_t& depth, chess::Board& b, int64_t& evaluate);
    //std::vector<engine::Engine::ScoredMove> getOrderedScoreMoveForCurrentPosition(chess::Board& b);
    //int64_t cleanSearchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply);
    int64_t searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply);

    // Genera tutte le mosse legali per la posizione corrente di b (nuova/bitboard)
    std::vector<chess::Board::Move> generateLegalMoves(const chess::Board& b) const;
    std::vector<ScoredMove> sortLegalMoves(const std::vector<chess::Board::Move>& moves, int ply, chess::Board& b, bool usIsWhite);

    int64_t evaluateCheckmate(const chess::Board& board);
/*
    int64_t avoidUnfavorableExchanges(int64_t bishopCount, int64_t knightCount, int64_t pawnCount);
    int64_t bonusBishopPair(int64_t bishopCount, int64_t knightCount) noexcept;
*/
    constexpr static int64_t NEG_INF = std::numeric_limits<int64_t>::min();
    constexpr static int64_t POS_INF = std::numeric_limits<int64_t>::max();
    constexpr static int32_t NEG_INF_32 = std::numeric_limits<int32_t>::min();
    constexpr static int32_t POS_INF_32 = std::numeric_limits<int32_t>::max();

    // Killer moves: up to 2 non-capture moves per ply that previously caused a beta cutoff
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
}; //class Engine final

} // namespace engine

#endif
