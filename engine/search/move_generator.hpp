#pragma once

#include <array>
#include <cstdint>

#include "../../board/board.hpp"
#include "../movelist.hpp"

namespace engine {
class Sorter;

class MoveGenerator final {

public:

    MoveGenerator() = delete;

        static MoveList<chess::Board::Move> generateLegalMoves(
            const chess::Board& b,
            bool inCheckKnown = false,
            bool inCheckValue = false,
            bool inDoubleCheckValue = false) noexcept;

    static MoveList<chess::Board::Move> generateTacticalMoves(
        const chess::Board& b,
        bool includeChecks = false,
        bool inCheckKnown = false,
        bool inCheckValue = false,
        bool inDoubleCheckValue = false) noexcept;

    static MoveList<chess::Board::Move> generateQSearchEvasions(const chess::Board& b) noexcept;

    static MoveList<chess::Board::Move> generateQSearchTacticalMoves(
        const chess::Board& b,
        int32_t standPat,
        int32_t alpha,
        int32_t beta,
        int ply,
        bool usIsWhite,
        int32_t searchDepth) noexcept;

private:

    static uint64_t betweenMaskExclusive(uint8_t from, uint8_t to) noexcept;

    static void addPromotionMoves(
        MoveList<chess::Board::Move>& moves,
        const chess::Coords& fromC,
        const chess::Coords& toC) noexcept;

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
        uint8_t piece) noexcept;

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
        uint64_t& pinnedMask,
        uint64_t pinRays[64]) noexcept;

    static void computeCheckEvasionMasks(
        const chess::Board& b,
        uint8_t color,
        uint64_t& evasionMask) noexcept;

    template<uint8_t PieceType>
    static void generateNonPawnLegalMoves(
        const chess::Board& b,
        MoveList<chess::Board::Move>& moves,
        uint64_t bb, uint64_t occ, uint64_t ownOcc,
        bool singleCheck, uint64_t evasionMask,
        uint64_t pinnedMask, const uint64_t pinRayBySquare[64],
        bool inCheck, bool inDoubleCheck, uint8_t pt) noexcept;

    template<uint8_t PieceType>
    static void generateNonPawnTacticalMoves(
        const chess::Board& b,
        MoveList<chess::Board::Move>& moves,
        uint64_t bb, uint64_t occ, uint64_t oppOcc, uint64_t evasionMask,
        uint64_t pinnedMask, const uint64_t pinRayBySquare[64],
        uint8_t piece, bool isWhite, chess::Coords enPassant, bool hasEnPassant) noexcept;
};

} // namespace engine
