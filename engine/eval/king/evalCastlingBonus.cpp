#include <bit>
#include "../evaluator.hpp"

namespace engine {

inline PhaseValue Evaluator::evalCastlingBonusSide(const chess::Board& b, int side) noexcept {
    const uint64_t kingBB = b.kings_bb[side];
    if (!kingBB) [[unlikely]] return {};

    const bool castleKs = b.getCastle(side == 0 ? 0 : 2);
    const bool castleQs = b.getCastle(side == 0 ? 1 : 3);
    const bool hasAnyRight = castleKs || castleQs;

    const int kingSq   = std::countr_zero(kingBB);
    const int kingFile = chess::Board::file(kingSq);
    const int kingRank = chess::Board::rank(kingSq);
    const int backRank = (side == 0) ? 7 : 0;   // own first rank (a1=56 => rank 7)
    const int startSq  = (side == 0) ? 60 : 4;  // e1 / e8
    const bool onBackRank = (kingRank == backRank);
    const bool tuckedWing = onBackRank && (kingFile <= 2 || kingFile >= 5);

    PhaseValue score{0, 0};
    if (tuckedWing && !hasAnyRight) {
        score += engine::CASTLE_PAWN_SUPPORT_BONUS;
    } else if (!hasAnyRight) {
        const bool central = !(kingFile <= 2 || kingFile >= 5);
        score -= central ? engine::LOSS_OF_CASTLING_PENALTY
                         : engine::KING_LOST_CASTLING_RIGHTS_PENALTY;
    } else if (kingSq == startSq) {
        score -= engine::KING_NON_CASTLING_PENALTY;
    }

    const int sign = (side == 0) ? 1 : -1;
    return sign * score;
}

PhaseValue Evaluator::evalCastlingBonus(const chess::Board& b) noexcept {
    return evalCastlingBonusSide(b, 0) + evalCastlingBonusSide(b, 1);
}

} // namespace engine
