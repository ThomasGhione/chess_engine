#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP

#include <cstdint>
#include "../../board/board.hpp"
#include "../movelist.hpp"

namespace engine {

class MoveGenerator final {
public:
    MoveGenerator() = delete;  // Static class, no instantiation
    
    static MoveList<chess::Board::Move> generateLegalMoves(const chess::Board& b) noexcept;
    
    static MoveList<chess::Board::Move> generateTacticalMoves(
        const chess::Board& b,
        bool includeChecks = false,
        bool inCheckKnown = false,
        bool inCheckValue = false,
        bool inDoubleCheckValue = false) noexcept;

private:
    
    static void addPawnMovesFromMask(
        const chess::Board& b,
        MoveList<chess::Board::Move>& moves,
        uint8_t from,
        uint64_t mask,
        bool inCheck,
        bool inDoubleCheck,
        uint8_t pawnPiece,
        chess::Coords enPassant,
        bool hasEnPassant) noexcept;
    
    static void addNonPawnMovesFromMask(
        const chess::Board& b,
        MoveList<chess::Board::Move>& moves,
        uint8_t from,
        uint64_t mask,
        bool inCheck,
        bool inDoubleCheck,
        uint8_t piece,
        const uint64_t* pinRays,
        chess::Coords kingPos) noexcept;
    
    static void addTacticalMovesFromMask(
        const chess::Board& b,
        uint64_t mask,
        uint8_t from,
        uint8_t piece,
        bool isPawn,
        bool isWhite,
        bool includeChecks,
        chess::Coords enPassant,
        bool hasEnPassant,
        MoveList<chess::Board::Move>& moves) noexcept;
    
    static void addTacticalMovesFromMaskInCheck(
        const chess::Board& b,
        uint64_t mask,
        uint8_t from,
        uint8_t piece,
        bool isPawn,
        bool isWhite,
        MoveList<chess::Board::Move>& moves) noexcept;
    
    static void computePinRays(
        const chess::Board& b,
        chess::Coords kingPos,
        bool isWhite,
        uint64_t occ,
        uint64_t& pinnedMask,
        uint64_t pinRays[64]) noexcept;
    
    static void computeCheckEvasionMasks(
        const chess::Board& b,
        uint8_t color,
        bool inCheck,
        bool inDoubleCheck,
        uint64_t& evasionMask) noexcept;
};

} // namespace engine

#endif // MOVEGEN_HPP
