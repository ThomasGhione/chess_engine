#include "../engine.hpp"
#include "../../tt/ttentry.hpp"

namespace engine {

void Engine::addTacticalMovesFromMask(const chess::Board& b,
                                     uint64_t mask,
                                     uint8_t from,
                                     uint8_t fromPiece,
                                     bool isPawn,
                                     bool isWhiteToMove,
                                     bool includeChecks,
                                     const chess::Coords& enPassant,
                                     bool inCheck,
                                     bool inDoubleCheck,
                                     MoveList<chess::Board::Move>& moves,
                                     bool skipLegalityCheck) noexcept {
    const chess::Coords fromC{from};
    const uint8_t oppColor = includeChecks
        ? chess::Board::oppositeColor(b.getActiveColor())
        : static_cast<uint8_t>(chess::Board::WHITE);
    const bool hasEnPassant = chess::Coords::isInBounds(enPassant);
    const uint8_t fromFile = static_cast<uint8_t>(from & 7);

    while (mask) {
        const uint8_t to = __builtin_ctzll(mask);
        mask &= (mask - 1);

        const uint8_t toPiece = b.get(to);
        const bool isEnPassant = isPawn && hasEnPassant && (to == enPassant.index)
            && (static_cast<uint8_t>(to & 7) != fromFile) && (toPiece == chess::Board::EMPTY);
        const bool isCapture = (toPiece != chess::Board::EMPTY) || isEnPassant;
        const bool isPromotion = isPawn && (chess::Board::rankOf(to) == chess::Board::promotionRank(isWhiteToMove));

        // Fast path: when checks are disabled, skip non-tactical quiet moves immediately.
        if (!includeChecks && !isCapture && !isPromotion) {
            continue;
        }

        const chess::Coords toC{to};

        if (!skipLegalityCheck || isEnPassant) {
            if (!b.isLegalPseudoMove(from, to, fromPiece, inCheck, inDoubleCheck)) {
                continue;
            }
        }

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
} // namespace engine
