#include "movegen.hpp"
#include "../../tt/ttentry.hpp"

namespace engine {

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
    const uint8_t fromFile = static_cast<uint8_t>(from & 7);
    const bool isWhite = (pawnPiece & chess::Board::MASK_COLOR) == chess::Board::WHITE;
    const uint8_t promotionRank = chess::Board::promotionRank(isWhite);

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);
        const chess::Coords toC{to};
        const bool isEnPassant = hasEnPassant
            && (toC == enPassant)
            && (static_cast<uint8_t>(to & 7) != fromFile);
        
        // Always check legality for en passant (changes occupancy), otherwise it's already filtered
        if (isEnPassant && !b.isLegalPseudoMove(from, to, pawnPiece, inCheck, inDoubleCheck)) {
            continue;
        }

        if (chess::Board::rankOf(to) == promotionRank) {
            moves.emplace_back(chess::Board::Move{fromC, toC, 'q'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'r'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'b'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'n'});
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
    uint8_t piece,
    [[maybe_unused]] const uint64_t* pinRays,
    [[maybe_unused]] chess::Coords kingPos) noexcept {
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
    const uint8_t fromFile = static_cast<uint8_t>(from & 7);

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);

        const uint8_t toPiece = b.get(to);
        const bool isEnPassant = isPawn && hasEnPassant && (to == enPassant.index)
            && (static_cast<uint8_t>(to & 7) != fromFile) && (toPiece == chess::Board::EMPTY);
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
            moves.emplace_back(chess::Board::Move{fromC, toC, 'q'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'r'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'b'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'n'});
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
            moves.emplace_back(chess::Board::Move{fromC, toC, 'q'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'r'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'b'});
            moves.emplace_back(chess::Board::Move{fromC, toC, 'n'});
            continue;
        }

        moves.emplace_back(chess::Board::Move{fromC, toC});
    }
}

} // namespace engine
