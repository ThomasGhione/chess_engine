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
        bool knownNotInCheck = false) noexcept;

    static MoveList<chess::Board::Move> generateLegalEvasions(
        const chess::Board& b,
        bool inDoubleCheckKnown = false,
        bool inDoubleCheckValue = false) noexcept;

    static MoveList<chess::Board::Move> generateTacticalMoves(
        const chess::Board& b) noexcept;

    static engine::MovePicker generateQSearchEvasions(
        const chess::Board& b,
        bool inDoubleCheckKnown = false,
        bool inDoubleCheckValue = false) noexcept;

    static engine::MovePicker generateQSearchTacticalMoves(
        const chess::Board& b,
        int32_t standPat,
        int32_t alpha,
        int ply) noexcept;

private:
    template<bool IsWhite>
    static void addPawnMovesFromMask(
        const chess::Board& b,
        MoveList<chess::Board::Move>& moves,
        int from,
        uint64_t mask,
        chess::Coords enPassant) noexcept;

    // Per-pawn pseudo-legal move emission shared by the full and evasion
    // generators. `evasionMask` must be ~0ULL when not in single check, so the
    // unconditional AND matches the full generator's `if (singleCheck)` guard.
    template<bool IsWhite>
    static void appendPawnPseudoLegalMoves(
        const chess::Board& b,
        MoveList<chess::Board::Move>& moves,
        uint64_t pawns,
        uint64_t occ,
        uint64_t oppOcc,
        uint64_t enPassantBit,
        chess::Coords enPassant,
        uint64_t evasionMask,
        uint64_t pinnedMask,
        const uint64_t pinRayBySquare[64]) noexcept;

    template<bool IsWhite>
    static MoveList<chess::Board::Move> generateLegalMovesFor(
        const chess::Board& b,
        bool knownNotInCheck) noexcept;

    template<bool IsWhite>
    static MoveList<chess::Board::Move> generateLegalEvasionsFor(
        const chess::Board& b,
        bool inDoubleCheckKnown,
        bool inDoubleCheckValue) noexcept;

    template<bool IsWhite>
    static MoveList<chess::Board::Move> generateTacticalMovesFor(
        const chess::Board& b) noexcept;

    template<bool IsWhite>
    static void computePinRays(
        const chess::Board& b,
        chess::Coords kingPos,
        uint64_t& pinnedMask,
        uint64_t pinRays[64]) noexcept;

    template<bool IsWhite>
    static void computeCheckEvasionMasks(
        const chess::Board& b,
        uint64_t& evasionMask) noexcept;

    template<bool HasPins, bool InCheck, uint8_t PieceType>
    static void generateNonPawnLegalMoves(
        MoveList<chess::Board::Move>& moves,
        uint64_t bb,
        uint64_t occ,
        uint64_t ownOcc,
        uint64_t evasionMask,
        uint64_t pinnedMask,
        const uint64_t pinRayBySquare[64]) noexcept;

    template<bool HasPins, bool InCheck>
    static void emitAllNonPawnLegal(
        MoveList<chess::Board::Move>& moves,
        uint64_t knights, uint64_t bishops, uint64_t rooks, uint64_t queens,
        uint64_t occ, uint64_t ownOcc, uint64_t evasionMask,
        uint64_t pinnedMask, const uint64_t pinRayBySquare[64]) noexcept;

    template<bool HasPins>
    static void emitAllNonPawnTactical(
        MoveList<chess::Board::Move>& moves,
        uint64_t knights, uint64_t bishops, uint64_t rooks, uint64_t queens,
        uint64_t occ, uint64_t oppOcc,
        uint64_t pinnedMask, const uint64_t pinRayBySquare[64]) noexcept;
};

} // namespace engine
