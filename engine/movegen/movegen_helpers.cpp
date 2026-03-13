#include "movegen.hpp"

namespace engine {

void MoveGenerator::addPromotionMoves(MoveList<chess::Board::Move>& moves,
                                      const chess::Coords& fromC,
                                      const chess::Coords& toC) noexcept {
    moves.emplace_back(fromC, toC, 'q');
    moves.emplace_back(fromC, toC, 'r');
    moves.emplace_back(fromC, toC, 'b');
    moves.emplace_back(fromC, toC, 'n');
}

// ============================================================================
// addPawnMovesFromMask
// ============================================================================
void MoveGenerator::addPawnMovesFromMask(
    const chess::Board& b,
    MoveList<chess::Board::Move>& moves,
    uint8_t from,
    uint64_t mask,
    bool inCheck,
    bool inDoubleCheck,
    uint8_t pawnPiece,
    chess::Coords enPassant,
    bool hasEnPassant) noexcept {
    if (!mask) [[unlikely]] return;
    
    const chess::Coords fromC{from};
    const uint8_t fromFile = chess::Board::fileOf(from);
    const bool isWhite = (b.getColor(from) == chess::Board::WHITE);
    const uint8_t promotionRank = chess::Board::promotionRank(isWhite);

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);
        const chess::Coords toC{to};
        const bool isEnPassant = hasEnPassant
            && (toC == enPassant)
            && (chess::Board::fileOf(to) != fromFile);
        
        // Always check legality for en passant (changes occupancy), otherwise it's already filtered
        if (isEnPassant && !b.isLegalPseudoMove(from, to, pawnPiece, inCheck, inDoubleCheck)) {
            continue;
        }

        if (chess::Board::rankOf(to) == promotionRank) {
            addPromotionMoves(moves, fromC, toC);
        } else {
            moves.emplace_back(chess::Board::Move{fromC, toC});
        }
    }
}

// ============================================================================
// addNonPawnMovesFromMask
// ============================================================================
void MoveGenerator::addNonPawnMovesFromMask(
    const chess::Board& b,
    MoveList<chess::Board::Move>& moves,
    uint8_t from,
    uint64_t mask,
    bool inCheck,
    bool inDoubleCheck,
    uint8_t piece) noexcept {
    if (!mask) [[unlikely]] return;

    const chess::Coords fromC{from};
    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);
        if (b.isLegalPseudoMove(from, to, piece, inCheck, inDoubleCheck)) {
            moves.emplace_back(chess::Board::Move{fromC, chess::Coords{to}});
        }
    }
}

// ============================================================================
// addTacticalMovesFromMask
// ============================================================================
void MoveGenerator::addTacticalMovesFromMask(
    const chess::Board& b,
    uint64_t mask,
    uint8_t from,
    uint8_t piece,
    bool isPawn,
    bool isWhite,
    bool includeChecks,
    chess::Coords enPassant,
    bool hasEnPassant,
    MoveList<chess::Board::Move>& moves) noexcept {
    const chess::Coords fromC{from};
    const uint8_t oppColor = includeChecks
        ? chess::Board::oppositeColor(b.getActiveColor())
        : static_cast<uint8_t>(chess::Board::WHITE);
    const uint8_t fromFile = chess::Board::fileOf(from);

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);

        const uint8_t toPiece = b.get(to);
        const bool isEnPassant = isPawn && hasEnPassant && (to == enPassant.index)
            && (chess::Board::fileOf(to) != fromFile) && (toPiece == chess::Board::EMPTY);
        const bool isCapture = (toPiece != chess::Board::EMPTY) || isEnPassant;
        const bool isPromotion = isPawn && (chess::Board::rankOf(to) == chess::Board::promotionRank(isWhite));

        // Fast path: when checks are disabled, skip non-tactical quiet moves immediately.
        if (!includeChecks && !isCapture && !isPromotion) {
            continue;
        }

        // Check legality for en passant and promotions
        if ((isEnPassant || isPromotion) && !b.isLegalPseudoMove(from, to, piece, false, false)) {
            continue;
        }

        const chess::Coords toC{to};

        if (isPromotion) {
            addPromotionMoves(moves, fromC, toC);
            continue;
        }

        bool shouldAdd = isCapture;

        if (!shouldAdd && includeChecks) {
            chess::Board::MoveState tmpState;
            const auto checkMove = chess::Board::Move{fromC, toC, '\0'};
            const_cast<chess::Board&>(b).doMove(checkMove, tmpState, '\0');
            if (const_cast<chess::Board&>(b).inCheck(oppColor)) {
                shouldAdd = true;
            }
            const_cast<chess::Board&>(b).undoMove(checkMove, tmpState);
        }

        if (shouldAdd) {
            moves.emplace_back(chess::Board::Move{fromC, toC});
        }
    }
}

// ============================================================================
// addTacticalMovesFromMaskInCheck
// ============================================================================
void MoveGenerator::addTacticalMovesFromMaskInCheck(
    const chess::Board& b,
    uint64_t mask,
    uint8_t from,
    uint8_t piece,
    bool isPawn,
    bool isWhite,
    MoveList<chess::Board::Move>& moves) noexcept {
    const chess::Coords fromC{from};

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);

        const bool isPromotion = isPawn && (chess::Board::rankOf(to) == chess::Board::promotionRank(isWhite));

        // In check evasion: all legal moves are tactical

        if (!b.isLegalPseudoMove(from, to, piece, true, false)) {
            continue;
        }

        const chess::Coords toC{to};

        if (isPromotion) {
            addPromotionMoves(moves, fromC, toC);
            continue;
        }

        moves.emplace_back(chess::Board::Move{fromC, toC});
    }
}

} // namespace engine
