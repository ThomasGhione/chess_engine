#include "../evaluator.hpp"

namespace engine {

inline int32_t Evaluator::evalCastlingBonusSide(const chess::Board& b, int side) noexcept {
    const uint64_t kingBB = b.kings_bb[side];
    if (!kingBB) [[unlikely]] return 0;

    const bool castleKs = b.getCastle(side == 0 ? 0 : 2);
    const bool castleQs = b.getCastle(side == 0 ? 1 : 3);
    const bool hasAnyRight = castleKs || castleQs;

    const int kingSq   = __builtin_ctzll(kingBB);
    const int kingFile = chess::Board::file(kingSq);
    const int kingRank = chess::Board::rank(kingSq);
    const int backRank = (side == 0) ? 7 : 0;   // own first rank (a1=56 => rank 7)
    const int startSq  = (side == 0) ? 60 : 4;  // e1 / e8
    const bool onBackRank = (kingRank == backRank);
    const bool tuckedWing = onBackRank && (kingFile <= 2 || kingFile >= 5);

    int32_t score = 0;
    if (tuckedWing && !hasAnyRight) {
        // King reached a wing on its own back rank with no rights left: it
        // has (almost certainly) castled. Reward the safe king.
        score += engine::CASTLE_PAWN_SUPPORT_BONUS;
    } else if (!hasAnyRight) {
        // Forfeited castling without tucking the king away.
        const bool central = !(kingFile <= 2 || kingFile >= 5);
        score -= central ? engine::LOSS_OF_CASTLING_PENALTY
                         : engine::KING_LOST_CASTLING_RIGHTS_PENALTY;
    } else if (kingSq == startSq) {
        // Still has rights but has not castled yet: gentle nudge. Cancels out
        // in symmetric positions; only bites the side lagging in king safety.
        score -= engine::KING_NON_CASTLING_PENALTY;
    }

    const int sign = (side == 0) ? 1 : -1;
    return sign * score;
}

int32_t Evaluator::evalCastlingBonus(const chess::Board& b) noexcept {
    return evalCastlingBonusSide(b, 0) + evalCastlingBonusSide(b, 1);
}

} // namespace engine
