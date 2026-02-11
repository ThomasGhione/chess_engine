#ifndef ENGINE_HPP
#define ENGINE_HPP

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
#endif

#include "../printer/menu.hpp"
#include "../printer/prints.hpp"
#include "../board/board.hpp"
#include "../coords/coords.hpp"

#include "basebonuspenaltyvalues.hpp"
#include "piecevaluetables.hpp"
#include "../tt/transposition_table.hpp"
#include "movelist.hpp"

namespace engine {

// ===================================================
// BITBOARD HELPERS
// ===================================================

/// Pop least significant bit and return its index
[[nodiscard]] inline uint8_t popLSB(uint64_t& bb) noexcept {
    const uint8_t index = static_cast<uint8_t>(__builtin_ctzll(bb));
    bb &= (bb - 1);
    return index;
}
class Engine final {
public:
    // Structs and enums
    struct ScoredMove {
        chess::Board::Move move;
        int64_t score;
    };

    enum GameResult : uint8_t {
        ONGOING = 0,
        WHITE_WINS = 1,
        BLACK_WINS = 2,
        DRAW = 3
    };
    //--- Structs and enums end

    //--- Constructors
    Engine();
    explicit Engine(const std::string& fen);
    
    // Engine is non-copyable and non-movable due to complex state
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;
    //--- Constructors end

    //--- Method
    inline static constexpr int manhattan(int a, int b) noexcept { return std::abs((a & 7) - (b & 7)) + std::abs((a >> 3) - (b >> 3)); };
    static int64_t evaluateCheckmate(const chess::Board& board) noexcept;
    static int64_t evalPawnStructure(uint64_t whitePawns, uint64_t blackPawns, bool isEndgame = false) noexcept;
    static int64_t evalBlockedCenterWithPieces(const chess::Board& b, uint64_t occ) noexcept;
    static int64_t evalRooks(uint64_t whiteRooks, uint64_t blackRooks, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalKingSafety(const chess::Board& b, uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalPieceCoordination(const chess::Board& b) noexcept;
    static int64_t evalOutposts(const chess::Board& b) noexcept;
    static int64_t evalKingActivity(const chess::Board& b, bool isEndgame) noexcept;
    static int64_t evalEndgameKingActivity(const chess::Board& b) noexcept;
    static int64_t evalCentralControl(uint64_t whitePawns, uint64_t blackPawns) noexcept;
    static int64_t evalCastlingBonus(const chess::Board& b) noexcept;
    static int64_t evalBadBishop(uint64_t bishops, uint64_t pawns, int side) noexcept;
    static int64_t evalMinorPieceDevelopment(const chess::Board& b) noexcept;
    static int64_t evalEarlyQueen(const chess::Board& b) noexcept;
    static int64_t evalBlockedPawnByBishops(const chess::Board& b) noexcept;
    static int64_t evalRookEndgamePressure(const chess::Board& b) noexcept;
    static int64_t evalQueenEndgamePressure(const chess::Board& b) noexcept;
    static int64_t evalDoubleRookEndgame(const chess::Board& b) noexcept;

    void reset() noexcept;
    bool movePiece(const chess::Coords from, const chess::Coords to, const char promotionPiece = '\0') noexcept;

    void search(uint64_t depth) noexcept;
    int64_t evaluate(const chess::Board& board) noexcept; 
    
    inline bool isGameOver() const noexcept { return gameResult != ONGOING; }
    inline bool isMate() const noexcept { return gameResult == WHITE_WINS || gameResult == BLACK_WINS; }
    bool isStalemate() const noexcept { return gameResult == DRAW; }
    void updateGameResult() noexcept;
    inline GameResult getGameResult() const noexcept { return gameResult; }
    inline uint8_t getActiveColor() const noexcept { return board.getActiveColor(); }
    
    __attribute__((always_inline))
    inline void addPsqt(uint64_t bbWhite, uint64_t bbBlack, const int64_t* table, int64_t& eval) noexcept {
        // White pieces: use index as-is
        while (bbWhite) {
            uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bbWhite));
            bbWhite &= (bbWhite - 1);
            eval += table[sq];
        }
        // Black pieces: mirror index vertically
        while (bbBlack) {
            uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bbBlack));
            bbBlack &= (bbBlack - 1);
            uint8_t idx = mirrorIndex(sq);
            eval -= table[idx];
        }
    }
    __attribute__((always_inline))
    inline static constexpr uint64_t adjacentFilesMask(int file) noexcept {
        uint64_t m = 0;
        if (file > 0) m |= chess::Board::fileMask(file - 1);
        if (file < 7) m |= chess::Board::fileMask(file + 1);
        return m;
    }
    inline static constexpr std::array<uint64_t, 64> initWhiteForwardFill() {
        std::array<uint64_t, 64> result{};
        for (int sq = 0; sq < 64; ++sq) {
            const int rank = chess::Board::rankOf(sq);
            // White pawns move toward decreasing rank (rank 0 is promotion).
            // Forward squares = all ranks strictly less than current rank.
            result[sq] = (rank > 0) ? ((chess::Board::bitMask(rank * 8)) - 1ULL) : 0ULL;
        }
        return result;
    }

    inline static constexpr std::array<uint64_t, 64> initBlackForwardFill() {
        std::array<uint64_t, 64> result{};
        for (int sq = 0; sq < 64; ++sq) {
            const int rank = chess::Board::rankOf(sq);
            // Black pawns move toward increasing rank (rank 7 is promotion).
            // Forward squares = all ranks strictly greater than current rank.
            result[sq] = (rank < 7) ? (0xFFFFFFFFFFFFFFFFULL << ((rank + 1) * 8)) : 0ULL;
        }
        return result;
    }
    // File masks (already defined in fileMask() but we precalculate for speed)
    inline static constexpr std::array<uint64_t, 8> FILE_MASKS = []() constexpr {
        std::array<uint64_t, 8> masks{};
        for (int f = 0; f < 8; ++f) {
            masks[f] = 0x0101010101010101ULL << f;
        }
        return masks;
    }();

    // Adjacent files ONLY (without center file) - optimization for isolated pawn check
    inline static constexpr std::array<uint64_t, 8> ADJACENT_FILES_ONLY = []() constexpr {
        std::array<uint64_t, 8> masks{};
        for (int f = 0; f < 8; ++f) {
            uint64_t m = 0;
            if (f > 0) m |= (0x0101010101010101ULL << (f - 1));
            if (f < 7) m |= (0x0101010101010101ULL << (f + 1));
            masks[f] = m;
        }
        return masks;
    }();

    // Precalculated adjacent files mask (including center file)
    inline static constexpr std::array<uint64_t, 8> ADJACENT_AND_FILE_MASKS = []() constexpr {
        std::array<uint64_t, 8> masks{};
        for (int f = 0; f < 8; ++f) {
            uint64_t m = (0x0101010101010101ULL << f); // center file
            if (f > 0) m |= (0x0101010101010101ULL << (f - 1)); // left
            if (f < 7) m |= (0x0101010101010101ULL << (f + 1)); // right
            masks[f] = m;
        }
        return masks;
    }();

    // King proximity masks (squares at distance <= 2 from each square)
    inline static constexpr std::array<uint64_t, 64> KING_PROXIMITY_MASKS = []() constexpr {
        std::array<uint64_t, 64> masks{};
        for (int sq = 0; sq < 64; ++sq) {
            uint64_t mask = 0;
            const int rank = chess::Board::rankOf(sq);
            const int file = chess::Board::fileOf(sq);
            
            // All squares within Manhattan distance 2
            for (int r = std::max(0, rank - 2); r <= std::min(7, rank + 2); ++r) {
                for (int f = std::max(0, file - 2); f <= std::min(7, file + 2); ++f) {
                    const int target = (r << 3) | f;
                    const int dist = std::abs(r - rank) + std::abs(f - file);
                    if (dist <= 2 && target != sq) {
                        mask |= chess::Board::bitMask(target);
                    }
                }
            }
            masks[sq] = mask;
        }
        return masks;
    }();

    template<bool IsEndgame>
    inline static constexpr int64_t evalInitiativeImpl(uint8_t activeColor) noexcept {
        constexpr int64_t bonus = IsEndgame ? INIT_BONUS_EG : INIT_BONUS_MG;
        return (activeColor == chess::Board::WHITE) ? bonus : -bonus;
    }

    __attribute__((hot, always_inline))
    inline int64_t evalInitiative(const chess::Board& b, bool isEndgame) noexcept {
        return isEndgame 
            ? evalInitiativeImpl<true>(b.getActiveColor()) 
            : evalInitiativeImpl<false>(b.getActiveColor());
    }

    template<int Side>
    inline static constexpr int64_t evalBadBishopImpl(uint64_t bishops, uint64_t pawns) noexcept {
        static_assert(Side == 0 || Side == 1, "Side must be 0 or 1");
        
        const int darkPawnCount = __builtin_popcountll(pawns & DARK_SQUARES);
        const int lightPawnCount = __builtin_popcountll(pawns & LIGHT_SQUARES);
        
        const int darkBishops = __builtin_popcountll(bishops & DARK_SQUARES);
        const int lightBishops = __builtin_popcountll(bishops & LIGHT_SQUARES);
        
        const int64_t score = -((darkBishops * darkPawnCount + lightBishops * lightPawnCount) * 8);
        
        if constexpr (Side == 0) {
            return score;
        } else {
            return -score;
        }
    }

    static int64_t getMaterialDelta(const chess::Board& b) noexcept;

    // DEBUG: Trace version of evaluate that prints each component
    int64_t evaluateTrace(const chess::Board& board) noexcept;

    // Magic bitboard initialization (shared across all Engine instances)
    static inline bool magicTablesInitialized = false;
    static void ensureMagicTablesInitialized() noexcept;

    // Legal move generation (bitboard-based)
    MoveList<chess::Board::Move> generateLegalMoves(const chess::Board& b) const noexcept;
    MoveList<ScoredMove> sortLegalMoves(const MoveList<chess::Board::Move>& moves, int ply, chess::Board& b, bool usIsWhite, uint64_t hashKey, const chess::Board::Move* previousMove = nullptr) noexcept;

    chess::Board::Move getBestMove(const MoveList<chess::Board::Move>& moves, bool searchBestMoveForWhite) noexcept;
    //--- Method end

    //--- Variabile
    chess::Board::Move bestMove;

    // Data members
    chess::Board board;
    bool isPlayerWhite;

    uint64_t depth;
    int64_t eval = 0;

    // Endgame depth extension flags (set once per game)
    bool depthExtendedMedium = false;  // +2 extension for <6 pieces
    bool depthExtendedMaximum = false; // +2 extension for 3 pieces

    // Transposition table
    tt::TranspositionTable tt;

    uint64_t nodesSearched = 0; 
    int32_t UCI_DEPTH = 0;
    static constexpr int32_t DEFAULTDEPTH = 10;
    static constexpr int32_t MAX_PLY = 64;
    std::string moveHistory = "";

#ifdef DEBUG
    // Transposition table statistics
    static uint64_t ttProbes;
    static uint64_t ttHits;
#endif

    int MAX_THREADS;
    // Dark/Light square masks for bad bishop evaluation
    inline static constexpr uint64_t DARK_SQUARES = 0xAA55AA55AA55AA55ULL;
    inline static constexpr uint64_t LIGHT_SQUARES = ~DARK_SQUARES;
    //--- Variabile end

private:
    //--- Structs and enums
    // Helper structures for cleaner function signatures
    struct SearchContext {
        int64_t depth;
        int64_t alpha;
        int64_t beta;
        int ply;
        uint8_t activeColor;
        const chess::Board::Move* previousMove = nullptr; // For counter-move history
    };

    struct AlphaBeta {
        int64_t alpha;
        int64_t beta;
    };

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
    //--- Structs and enums end

    //--- Variabile
    GameResult gameResult = Engine::ONGOING;
    constexpr static int64_t NEG_INF = std::numeric_limits<int64_t>::min();
    constexpr static int64_t POS_INF = std::numeric_limits<int64_t>::max();
    
    // Killer moves: up to 2 non-capture moves per ply that previously caused a beta cutoff
    chess::Board::Move killerMoves[2][MAX_PLY] {};

    // History heuristic: bonus for non-capture moves that often cause cutoffs
    // history[colorIndex][fromIndex][toIndex]
    int history[2][64][64] = {};

    // Counter-move history: tracks best response to opponent's previous move
    // counterMoves[prevFrom][prevTo] → best response move
    // Improves move ordering in tactical sequences
    chess::Board::Move counterMoves[64][64] {};

    // Capture history: bonus for captures that often cause cutoffs
    // captureHistory[color][to][victimType]
    int captureHistory[2][64][7] = {};

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
    //--- Variabile end

    //--- Method
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
    static int64_t evalKingAttackZone(const chess::Board& b, const AttackData data[2]) noexcept;

    // Initial best score for min-max search
    // White maximizes: starts from -INF, Black minimizes: starts from +INF
    template<bool IsWhite>
    static constexpr int64_t initialBest() noexcept {
        return IsWhite ? NEG_INF : POS_INF;
    }
    // Runtime version (when color is not known at compile time)
    static constexpr int64_t initialBest(bool isWhite) noexcept {
        return isWhite ? NEG_INF : POS_INF;
    }
    
    // Compare if newScore is better than currentBest (color-aware)
    // White: newScore > currentBest (maximize)
    // Black: newScore < currentBest (minimize)
    template<bool IsWhite>
    static constexpr bool isBetter(int64_t newScore, int64_t currentBest) noexcept {
        return IsWhite ? (newScore > currentBest) : (newScore < currentBest);
    }
    // Runtime version
    static constexpr bool isBetter(int64_t newScore, int64_t currentBest, bool isWhite) noexcept {
        return isWhite ? (newScore > currentBest) : (newScore < currentBest);
    }
    
    // Check if we have a beta cutoff (position too good, opponent won't allow it)
    // White (maximizer): score >= beta
    // Black (minimizer): score <= alpha
    __attribute__((always_inline))
    static inline bool isBetaCutoff(int64_t score, int64_t alpha, int64_t beta, bool isWhite) noexcept {
        return isWhite ? (score >= beta) : (score <= alpha);
    }
    
    // Update alpha or beta bound based on score
    // White (maximizer): alpha = max(alpha, score)
    // Black (minimizer): beta = min(beta, score)
    __attribute__((always_inline))
    static inline void updateBound(int64_t score, int64_t& alpha, int64_t& beta, bool isWhite) noexcept {
        if (isWhite) {
            if (score > alpha) alpha = score;
        } else {
            if (score < beta) beta = score;
        }
    }
    
    // Check delta pruning condition
    // White: standPat + margin < alpha (can't reach alpha even with best capture)
    // Black: standPat - margin > beta (can't reach beta even with best capture)
    __attribute__((always_inline))
    static inline bool shouldDeltaPrune(int64_t standPat, int64_t margin, int64_t alpha, int64_t beta, bool isWhite) noexcept {
        return isWhite ? (standPat + margin < alpha) : (standPat - margin > beta);
    }
    
    // Return the cutoff value when beta cutoff occurs
    // White: return beta, Black: return alpha
    __attribute__((always_inline))
    static inline int64_t cutoffValue(int64_t alpha, int64_t beta, bool isWhite) noexcept {
        return isWhite ? beta : alpha;
    }

    // Fast access to piece values (inline for zero-cost abstraction)
    __attribute__((always_inline))
    static inline constexpr int64_t getPieceValue(uint8_t pieceType) noexcept {
        return PIECE_VALUES[pieceType & chess::Board::MASK_PIECE_TYPE];
    }

    void doMoveInBoard(chess::Board::Move bestMove) noexcept;
    void updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& bestScore, 
                 chess::Board::Move& bestMove, const chess::Board::Move& m) noexcept;
    void updateMinMax(bool usIsWhite, int64_t score, int64_t& alpha, int64_t& beta, int64_t& best) noexcept;

    void updateKillerAndHistoryOnBetaCutoff(const chess::Board& b, const chess::Board::Move& m, int64_t depth, int ply, uint8_t us, int64_t alpha, int64_t beta, int (&history)[2][64][64], chess::Board::Move (&killerMoves)[2][Engine::MAX_PLY], const chess::Board::Move* previousMove = nullptr) noexcept;

    // Search helpers
    bool handleSearchPrelude(const int64_t& depth, const AlphaBeta& bounds, int64_t& score, uint64_t hashKey) noexcept;
    ScoredMove searchMoves(chess::Board& b, const MoveList<ScoredMove>& orderedScoredMoves,
                          bool usIsWhite, const SearchContext& ctx, AlphaBeta& bounds, bool allowUpdates, bool allowTTWrite = true) noexcept;
    
    // Move scoring helpers
    void addMVVLVABonus(const chess::Board::Move& m, const chess::Board& b, int64_t& score) noexcept;
    void addPromotionBonus(const chess::Board::Move& m, uint8_t pieceType, bool usIsWhite, int64_t& score) noexcept;
    void addCheckBonus(const chess::Board::Move& m, chess::Board& b, bool usIsWhite, int64_t& score) noexcept;
    void addKillerAndHistoryBonus(const chess::Board::Move& m, int ply, bool usIsWhite, int64_t& score) noexcept;
    void addKingMoveBonus(const chess::Board::Move& m, uint8_t pieceType, bool inCheck, int fullMoveClock, int64_t& score) noexcept;
    int64_t staticExchangeEvaluation(const chess::Board& b, const chess::Board::Move& m) const noexcept;

    int64_t searchPosition(chess::Board& b, int64_t depth, int64_t alpha, int64_t beta, int ply, bool useTT = true, bool allowTTWrite = true) noexcept;
    int64_t quiescenceSearch(chess::Board& b, int64_t alpha, int64_t beta, int ply) noexcept;
    bool isKillerMove(const chess::Board::Move& m, const chess::Board::Move killerMoves[2][Engine::MAX_PLY], int ply) const noexcept;
    
    // Quiescence helper: generates only tactical moves (captures, promotions)
    MoveList<chess::Board::Move> generateTacticalMoves(const chess::Board& b, bool includeChecks = false) const noexcept;
    //--- Method end
}; //class Engine final
} // namespace engine
#endif
