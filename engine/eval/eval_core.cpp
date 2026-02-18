#include "evaluator.hpp"
#include "../piecevaluetables.hpp"

namespace engine {

const std::array<uint64_t, 8> Evaluator::FILE_MASKS = Evaluator::initFileMasks();
const std::array<uint64_t, 8> Evaluator::ADJACENT_FILES_ONLY = Evaluator::initAdjacentFilesOnly();
const std::array<uint64_t, 8> Evaluator::ADJACENT_AND_FILE_MASKS = Evaluator::initAdjacentAndFileMasks();
const std::array<uint64_t, 64> Evaluator::KING_PROXIMITY_MASKS = Evaluator::initKingProximityMasks();

const std::array<uint64_t, 64> Evaluator::WHITE_FORWARD_FILL = Evaluator::initWhiteForwardFill();
const std::array<uint64_t, 64> Evaluator::BLACK_FORWARD_FILL = Evaluator::initBlackForwardFill();

int64_t Evaluator::getMaterialDelta(const chess::Board& b) noexcept {
    return static_cast<int64_t>(
          (__builtin_popcountll(b.pawns_bb[0])   - __builtin_popcountll(b.pawns_bb[1]))   * PIECE_VALUES[chess::Board::PAWN]
        + (__builtin_popcountll(b.knights_bb[0]) - __builtin_popcountll(b.knights_bb[1])) * PIECE_VALUES[chess::Board::KNIGHT]
        + (__builtin_popcountll(b.bishops_bb[0]) - __builtin_popcountll(b.bishops_bb[1])) * PIECE_VALUES[chess::Board::BISHOP]
        + (__builtin_popcountll(b.rooks_bb[0])   - __builtin_popcountll(b.rooks_bb[1]))   * PIECE_VALUES[chess::Board::ROOK]
        + (__builtin_popcountll(b.queens_bb[0])  - __builtin_popcountll(b.queens_bb[1]))  * PIECE_VALUES[chess::Board::QUEEN]
        + (__builtin_popcountll(b.kings_bb[0])   - __builtin_popcountll(b.kings_bb[1]))   * PIECE_VALUES[chess::Board::KING]);
}

void Evaluator::addPsqt(uint64_t bbWhite, uint64_t bbBlack, const int64_t* table, int64_t& eval) noexcept {
    while (bbWhite) {
        const uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bbWhite));
        bbWhite &= (bbWhite - 1);
        eval += table[sq];
    }
    while (bbBlack) {
        const uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bbBlack));
        bbBlack &= (bbBlack - 1);
        const uint8_t idx = mirrorIndex(sq);
        eval -= table[idx];
    }
}

int64_t Evaluator::evalInitiative(const chess::Board& b, bool isEndgame) noexcept {
    return isEndgame
        ? evalInitiativeImpl<true>(b.getActiveColor())
        : evalInitiativeImpl<false>(b.getActiveColor());
}

} // namespace engine
