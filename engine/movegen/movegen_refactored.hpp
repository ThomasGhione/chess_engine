#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP

#include <cstdint>
#include "../../board/board.hpp"
#include "../movelist.hpp"

namespace engine {

/**
 * @brief Move generation engine - Stateless utility class for generating chess moves
 */
class MoveGenerator final {
public:
    MoveGenerator() = delete;  // Static class, no instantiation
    
    /**
     * @brief Context for position state during move generation
     * Groups position-derived information to reduce parameter passing
     */
    struct PositionContext {
        bool isWhite;
        bool inCheck;
        bool inDoubleCheck;
        chess::Coords enPassant;
        bool hasEnPassant;
        uint8_t color;
        
        PositionContext(const chess::Board& b) noexcept 
            : color(b.getActiveColor())
            , isWhite(chess::Board::colorToIndex(color) == 0)
            , inCheck(b.inCheck(color))
            , inDoubleCheck(inCheck && b.isDoubleCheck(color))
            , enPassant(b.getEnPassant())
            , hasEnPassant(chess::Coords::isInBounds(enPassant))
        {}
    };
    
    /**
     * @brief Generate all legal moves for the current position
     */
    static MoveList<chess::Board::Move> generateLegalMoves(const chess::Board& b) noexcept;
    
    /**
     * @brief Generate only tactical moves (captures, promotions, optional checks)
     */
    static MoveList<chess::Board::Move> generateTacticalMoves(
        const chess::Board& b,
        bool includeChecks = false,
        bool inCheckKnown = false,
        bool inCheckValue = false,
        bool inDoubleCheckValue = false) noexcept;

private:
    // ========================================================================
    // Helper functions for move generation
    // ========================================================================
    
    /**
     * @brief Add pawn moves from a bitboard mask
     */
    static void addPawnMovesFromMask(
        const chess::Board& b,
        MoveList<chess::Board::Move>& moves,
        uint8_t from,
        uint64_t mask,
        const PositionContext& ctx,
        uint8_t pawnPiece) noexcept;
    
    /**
     * @brief Add non-pawn piece moves from a bitboard mask
     */
    static void addNonPawnMovesFromMask(
        const chess::Board& b,
        MoveList<chess::Board::Move>& moves,
        uint8_t from,
        uint64_t mask,
        const PositionContext& ctx,
        uint8_t piece) noexcept;
    
    /**
     * @brief Add tactical moves (captures/promotions) from a bitboard mask
     */
    static void addTacticalMovesFromMask(
        const chess::Board& b,
        MoveList<chess::Board::Move>& moves,
        uint8_t from,
        uint64_t mask,
        uint8_t piece,
        bool isPawn,
        const PositionContext& ctx,
        bool includeChecks) noexcept;
    
    /**
     * @brief Add tactical moves when in check (evasions only)
     */
    static void addTacticalMovesFromMaskInCheck(
        const chess::Board& b,
        MoveList<chess::Board::Move>& moves,
        uint8_t from,
        uint64_t mask,
        uint8_t piece,
        bool isPawn,
        const PositionContext& ctx) noexcept;
    
    /**
     * @brief Compute pin rays for all pieces
     */
    static void computePinRays(
        const chess::Board& b,
        chess::Coords kingPos,
        bool isWhite,
        uint64_t& pinnedMask,
        uint64_t pinRays[64]) noexcept;
    
    /**
     * @brief Compute check evasion mask for single check
     */
    static void computeCheckEvasionMasks(
        const chess::Board& b,
        const PositionContext& ctx,
        uint64_t& evasionMask) noexcept;
};

} // namespace engine

#endif // MOVEGEN_HPP
