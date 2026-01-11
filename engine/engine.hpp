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
    explicit Engine(const std::string& fen);
    
    // Engine non è copiabile né movibile a causa di stato complesso
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    chess::Board board;
    bool isPlayerWhite;
    bool isCheckMate;

    uint64_t depth;

    int64_t eval = 0;  // Inizializzato a 0 per evitare valori spazzatura

    // Traccia se la depth è stata estesa per l'endgame (una volta per partita)
    bool depthExtendedMedium = false;  // Estensione +2 per <6 pezzi
    bool depthExtendedMaximum = false; // Estensione +2 per 3 pezzi

    // Puntatore alla transposition table globale
    TTEntry* ttTable;

    static uint64_t nodesSearched; 

    static constexpr int32_t DEFAULTDEPTH = 10;
    static std::string moveHistory;

#ifdef DEBUG
    // Transposition table statistics (only in debug builds)
    static uint64_t ttProbes;
    static uint64_t ttHits;
#endif

    bool movePiece(const chess::Coords from, const chess::Coords to, const char promotionPiece = '\0') noexcept;

    void search(uint64_t depth) noexcept;
    int64_t evaluate(const chess::Board& board) noexcept; 
    
    bool isMate() noexcept;
    void setIsCheckMate() noexcept;

    static int64_t getMaterialDelta(const chess::Board& b) noexcept;

    static constexpr int MAX_PLY = 64;

    struct ScoredMove {
        chess::Board::Move move;
        int64_t score;
    };

    void reset() noexcept;




    // Genera tutte le mosse legali per la posizione corrente di b (nuova/bitboard)
    MoveList<chess::Board::Move> generateLegalMoves(const chess::Board& b) const noexcept;
    MoveList<ScoredMove> sortLegalMoves(const MoveList<chess::Board::Move>& moves, int ply, chess::Board& b, bool usIsWhite) noexcept;

    chess::Board::Move getBestMove(const MoveList<chess::Board::Move>& moves, bool searchBestMoveForWhite) noexcept;

private:
    int MAX_THREADS;

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

    bool shouldPruneLateMove(chess::Board& b,const chess::Board::Move& m, int64_t depth, bool inCheck, bool usIsWhite, int moveIndex, int totalMoves) noexcept;

    void updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m, int64_t depth, int ply, uint8_t us, int64_t alpha, int64_t beta, int (&history)[2][64][64], chess::Board::Move (&killerMoves)[2][Engine::MAX_PLY]) noexcept;

    // Helper methods for search
    bool handleSearchPrelude(const chess::Board& b, const int64_t& depth, const AlphaBeta& bounds, int64_t& score, uint64_t hashKey) noexcept;
    ScoredMove searchMoves(chess::Board& b, const MoveList<ScoredMove>& orderedScoredMoves,
                          bool usIsWhite, const SearchContext& ctx, AlphaBeta& bounds, bool allowUpdates) noexcept;
    bool probeTTCache(uint64_t hashKey, int64_t depth, const AlphaBeta& bounds, int64_t& score) noexcept;
    void saveTTEntry(const TTSaveInfo& info) noexcept;
    
    // Helper methods for move execution
    // REMOVED: executeMove() was redundant - use doMove() with promotion check inline
    void undoAndUpdateMove(const chess::Board::Move& m, const chess::Board::MoveState& state, bool usIsWhite,
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
    int64_t searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply, bool useTT = true, bool allowTTWrite = true) noexcept;
    bool isKillerMove(const chess::Board::Move& m, const chess::Board::Move killerMoves[2][Engine::MAX_PLY], int ply) const noexcept;

public:
    static int manhattan(int a, int b) noexcept;
    static int64_t evaluateCheckmate(const chess::Board& board) noexcept;
    static int64_t evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame = false) noexcept;
    static int64_t evalBlockedCenterWithPieces(const chess::Board& b, uint64_t occ) noexcept;
    static int64_t evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalKingActivity(const chess::Board& b, bool isEndgame) noexcept;
    static int64_t evalBadKingPosition(const chess::Board& b) noexcept;
    static int64_t evalEndgameKingActivity(const chess::Board& b) noexcept;
    static int64_t evalCentralControl(uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalCastlingBonus(const chess::Board& b) noexcept;
    static int64_t evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept;
    static int64_t evalMinorPieceDevelopment(const chess::Board& b) noexcept;
    static int64_t evalEarlyKing(const chess::Board& b) noexcept;
    static int64_t evalEarlyRook(const chess::Board& b) noexcept;
    static int64_t evalPassiveRooks(const chess::Board& b, uint64_t occ) noexcept;
    static int64_t evalEarlyQueen(const chess::Board& b) noexcept;
    static int64_t evalInitiative(const chess::Board& b, bool isEndgame) noexcept;
    static int64_t evalKnightOnRim(const chess::Board& b) noexcept;

    /*
    int64_t avoidUnfavorableExchanges(int64_t bishopCount, int64_t knightCount, int64_t pawnCount);
    int64_t bonusBishopPair(int64_t bishopCount, int64_t knightCount) noexcept;
*/
private:
    // Attack data structure for evaluation optimization
    struct AttackData {
        uint64_t allAttacks;
        uint64_t pawnAttacks;
        uint64_t knightAttacks;
        uint64_t bishopAttacks;
        uint64_t rookAttacks;
        uint64_t queenAttacks;

        int64_t knightMobility;
        int64_t bishopMobility;
        int64_t rookMobility;
        int64_t queenMobility;
        
        bool isComputed; // Lazy evaluation flag
    };

    // Helper function to compute attack data once
    static void computeAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept;
    
    // Lazy evaluation: compute only if needed
    __attribute__((always_inline))
    static inline void ensureAttackData(AttackData data[2], const chess::Board& b, uint64_t occ) noexcept {
        if (!data[0].isComputed) {
            computeAttackData(data, b, occ);
        }
    }

    // Evaluation helper functions using precomputed attack data
    static int64_t evalMobility(const AttackData data[2]) noexcept;
    static int64_t evalTrappedPieces(const chess::Board& b, uint64_t occ) noexcept;
    static int64_t evalHangingPieces(const chess::Board& b, const AttackData data[2]) noexcept;

    constexpr static int64_t NEG_INF = std::numeric_limits<int64_t>::min();
    constexpr static int64_t POS_INF = std::numeric_limits<int64_t>::max();

    // Killer moves: up to 2 non-capture moves per ply that previously caused a beta cutoff
    chess::Board::Move killerMoves[2][MAX_PLY] {};

    // History heuristic: bonus for non-capture moves that often cause cutoffs
    // history[colorIndex][fromIndex][toIndex]
    int history[2][64][64] = {};

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
