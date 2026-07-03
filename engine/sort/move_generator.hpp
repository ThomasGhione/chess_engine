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

    static MoveList generateLegalMoves(
        const chess::Board& b,
        bool knownNotInCheck = false) noexcept;

    // `checkers` is the (non-empty) Board::checkersTo bitboard the caller
    // already computed; it also yields the evasion mask without a rescan.
    static MoveList generateLegalEvasions(
        const chess::Board& b,
        uint64_t checkers) noexcept;

    static MoveList generateTacticalMoves(
        const chess::Board& b) noexcept;

    static engine::MovePicker generateQSearchEvasions(
        const chess::Board& b,
        uint64_t checkers) noexcept;

    static engine::MovePicker generateQSearchTacticalMoves(
        const chess::Board& b,
        int32_t standPat,
        int32_t alpha,
        int ply) noexcept;

private:
    template<bool IsWhite>
    static void addPawnMovesFromMask(
        const chess::Board& b,
        MoveList& moves,
        int from,
        uint64_t mask,
        chess::Square enPassant) noexcept;

    // Per-pawn pseudo-legal move emission shared by the full and evasion
    // generators. `evasionMask` must be ~0ULL when not in single check, so the
    // unconditional AND matches the full generator's `if (singleCheck)` guard.
    template<bool IsWhite>
    static void appendPawnPseudoLegalMoves(
        const chess::Board& b,
        MoveList& moves,
        uint64_t pawns,
        uint64_t occ,
        uint64_t oppOcc,
        uint64_t enPassantBit,
        chess::Square enPassant,
        uint64_t evasionMask,
        uint64_t pinnedMask,
        const uint64_t pinRayBySquare[64]) noexcept;

    // Check state handed to the unified generator. When `known`, `checkers`
    // is authoritative (0 = not in check) and no attack scan is needed.
    struct CheckContext {
        bool     known    = false;
        uint64_t checkers = 0;
    };

    // One generator for both the normal and the in-check (evasion) case: the
    // evasion mask, double-check early-out and !inCheck castling gate already
    // specialize the output, so a separate evasion body would be a duplicate.
    template<bool IsWhite>
    static MoveList generateLegalMovesFor(
        const chess::Board& b,
        CheckContext check) noexcept;

    template<bool IsWhite>
    static MoveList generateTacticalMovesFor(
        const chess::Board& b) noexcept;

    template<bool IsWhite>
    static uint64_t computePinRays(
        const chess::Board& b,
        chess::Square kingPos,
        uint64_t pinRays[64]) noexcept;

    template<bool HasPins, bool InCheck, uint8_t PieceType>
    static void generateNonPawnLegalMoves(
        MoveList& moves,
        uint64_t bb,
        uint64_t occ,
        uint64_t ownOcc,
        uint64_t evasionMask,
        uint64_t pinnedMask,
        const uint64_t pinRayBySquare[64]) noexcept;

    template<bool HasPins, bool InCheck>
    static void emitAllNonPawnLegal(
        MoveList& moves,
        uint64_t knights, uint64_t bishops, uint64_t rooks, uint64_t queens,
        uint64_t occ, uint64_t ownOcc, uint64_t evasionMask,
        uint64_t pinnedMask, const uint64_t pinRayBySquare[64]) noexcept;

    template<bool HasPins>
    static void emitAllNonPawnTactical(
        MoveList& moves,
        uint64_t knights, uint64_t bishops, uint64_t rooks, uint64_t queens,
        uint64_t occ, uint64_t oppOcc,
        uint64_t pinnedMask, const uint64_t pinRayBySquare[64]) noexcept;
};

} // namespace engine
