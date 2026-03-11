#include "movegen.hpp"
#include "../../tt/ttentry.hpp"
#include "../inl/bitboard_helpers.inl"

namespace engine {

MoveList<chess::Board::Move>
MoveGenerator::generateLegalMoves(const chess::Board& b) noexcept {
    MoveList<chess::Board::Move> moves;

    const uint8_t color = b.getActiveColor();
    const int side = chess::Board::colorToIndex(color);
    const bool isWhite = (side == 0);

    const uint64_t occ = b.getPiecesBitMap();

    const uint64_t pawns   = b.pawns_bb[side];
    const uint64_t knights = b.knights_bb[side];
    const uint64_t bishops = b.bishops_bb[side];
    const uint64_t rooks   = b.rooks_bb[side];
    const uint64_t queens  = b.queens_bb[side];
    const uint64_t kings   = b.kings_bb[side];

    const uint64_t ownOcc = pawns | knights | bishops | rooks | queens | kings;
    const uint64_t oppOcc = occ & ~ownOcc;
    const chess::Coords enPassant = b.getEnPassant();
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);
    const uint64_t enPassantBit = hasEnPassant ? chess::Board::bitMask(enPassant.index) : 0ULL;
    const bool inCheck = b.inCheck(color);
    const bool inDoubleCheck = inCheck && b.isDoubleCheck(color);
    const bool singleCheck = inCheck && !inDoubleCheck;
    const uint8_t pawnPiece = static_cast<uint8_t>(chess::Board::PAWN | color);
    const uint8_t knightPiece = static_cast<uint8_t>(chess::Board::KNIGHT | color);
    const uint8_t bishopPiece = static_cast<uint8_t>(chess::Board::BISHOP | color);
    const uint8_t rookPiece = static_cast<uint8_t>(chess::Board::ROOK | color);
    const uint8_t queenPiece = static_cast<uint8_t>(chess::Board::QUEEN | color);
    const uint8_t kingPiece = static_cast<uint8_t>(chess::Board::KING | color);
    
    uint64_t evasionMask = ~0ULL;
    if (singleCheck) {
        computeCheckEvasionMasks(b, color, inCheck, inDoubleCheck, evasionMask);
    }

    // ================= KING =================
    if (!kings) [[unlikely]] return moves; // No king found, return empty move list
    
    const uint8_t from = static_cast<uint8_t>(__builtin_ctzll(kings));
    const chess::Coords fromC{from};

    // King moves MUST always check legality (can't move to attacked squares)
    uint64_t mask = pieces::KING_ATTACKS[from] & ~ownOcc;
    while (mask) {
        const uint8_t to = popLSB(mask);
        if (b.isLegalPseudoMove(from, to, kingPiece, inCheck, inDoubleCheck)) {
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
        }
    }

    if (!inCheck) { // castling: illegal when in check.
        const uint8_t f = from & 7;
        if (f <= 5 && b.isLegalPseudoMove(from, static_cast<uint8_t>(from + 2), inCheck))
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{static_cast<uint8_t>(from + 2)}});
        if (f >= 2 && b.isLegalPseudoMove(from, static_cast<uint8_t>(from - 2), inCheck))
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{static_cast<uint8_t>(from - 2)}});
    }

    // In double-check only king moves are legal.
    if (inDoubleCheck) return moves;
    
    uint64_t pinnedMask = 0ULL;
    std::array<uint64_t, 64> pinRayBySquare{};
    if (pawns | knights | bishops | rooks | queens) [[likely]] {
        computePinRays(b, fromC, isWhite, occ, pinnedMask, pinRayBySquare.data());
    }

    // NOTE: for performance, legality checks are skipped for many non-king moves
    // when check/pin filters already guarantee king safety.
    
    uint64_t bb = pawns;
    while (bb) {
        const uint8_t from = popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::getPawnForwardPushes(from, isWhite, occ);
        uint64_t caps = pieces::PAWN_ATTACKS[isWhite][from] & oppOcc;
        uint64_t epCandidate = 0ULL;
        if (hasEnPassant && (pieces::PAWN_ATTACKS[isWhite][from] & enPassantBit)) {
            caps |= enPassantBit;
            epCandidate = enPassantBit;
        }
        mask |= caps;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        if (epCandidate) {
            // Keep EP candidate for legality check because EP changes occupancy on two squares.
            mask |= epCandidate;
        }
        addPawnMovesFromMask(b, moves, from, mask, inCheck, inDoubleCheck, pawnPiece, enPassant, hasEnPassant);
    }

    bb = knights;
    while (bb) {
        const uint8_t from = popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::KNIGHT_ATTACKS[from] & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        addNonPawnMovesFromMask(b, moves, from, mask, inCheck, inDoubleCheck,
                                knightPiece, pinRayBySquare.data(), fromC);
    }

    bb = bishops;
    while (bb) {
        const uint8_t from = popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::getBishopAttacks(from, occ) & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        addNonPawnMovesFromMask(b, moves, from, mask, inCheck, inDoubleCheck,
                                bishopPiece, pinRayBySquare.data(), fromC);
    }

    bb = rooks;
    while (bb) {
        const uint8_t from = popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::getRookAttacks(from, occ) & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        addNonPawnMovesFromMask(b, moves, from, mask, inCheck, inDoubleCheck,
                                rookPiece, pinRayBySquare.data(), fromC);
    }

    bb = queens;
    while (bb) {
        const uint8_t from = popLSB(bb);
        const uint64_t fromBit = chess::Board::bitMask(from);
        const bool isPinned = (pinnedMask & fromBit) != 0ULL;
        uint64_t mask = pieces::getQueenAttacks(from, occ) & ~ownOcc;
        if (singleCheck) mask &= evasionMask;
        if (isPinned) mask &= pinRayBySquare[from];
        addNonPawnMovesFromMask(b, moves, from, mask, inCheck, inDoubleCheck,
                                queenPiece, pinRayBySquare.data(), fromC);
    }

    return moves;
}
} // namespace engine
