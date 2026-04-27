#pragma once

#include <array>
#include <cstdint>

#include "../../board/board.hpp"
#include "../movelist.hpp"
#include "sorter.hpp"

namespace engine {

class MoveGenerator final {

public:

    MoveGenerator() = delete;

        static MoveList<chess::Board::Move> generateLegalMoves(
            const chess::Board& b,
            bool inCheckKnown = false,
            bool inCheckValue = false,
            bool inDoubleCheckValue = false) noexcept;

    static MoveList<chess::Board::Move> generateTacticalMoves(
        const chess::Board& b) noexcept;

    static engine::Sorter::MovePickerData generateQSearchEvasions(const chess::Board& b) noexcept;

    static engine::Sorter::MovePickerData generateQSearchTacticalMoves(
        const chess::Board& b,
        int32_t standPat,
        int32_t alpha,
        int32_t beta,
        int ply,
        bool usIsWhite,
        int32_t searchDepth) noexcept;

private:

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
        bool isWhite,
        uint8_t promotionRank,
        chess::Coords enPassant,
        bool hasEnPassant) noexcept;

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

    template<bool InCheck, uint8_t PieceType>
    static void generateNonPawnLegalMoves(
        MoveList<chess::Board::Move>& moves,
        uint64_t bb,
        uint64_t occ,
        uint64_t ownOcc,
        uint64_t evasionMask,
        uint64_t pinnedMask,
        const uint64_t pinRayBySquare[64]) noexcept;

};

} // namespace engine
