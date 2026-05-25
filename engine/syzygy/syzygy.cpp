#include "syzygy.hpp"

extern "C" {
#include "tbprobe.h"
}

#include <vector>

namespace syzygy {

// TB win/loss scores sit just inside MATE_BOUND so they're treated as
// decisive but ranked below actual checkmates. Blessed/cursed results
// (50-move rule) are scored as 0 (draw).
static constexpr int32_t TB_WIN_SCORE  =  std::numeric_limits<int32_t>::max() - 2048 - 500;
static constexpr int32_t TB_LOSS_SCORE = -TB_WIN_SCORE;

bool SyzygyProber::load(const std::string& path) {
    loaded_ = tb_init(path.c_str());
    return loaded_;
}

int SyzygyProber::maxPieces() const noexcept {
    return TB_LARGEST;
}

bool SyzygyProber::inTBRange(const chess::Board& board) const noexcept {
    if (!loaded_) return false;
    // All pieces regardless of color — popcount of the full occupancy.
    const int pieces = __builtin_popcountll(
        board.pawns_bb[0]   | board.pawns_bb[1]   |
        board.knights_bb[0] | board.knights_bb[1] |
        board.bishops_bb[0] | board.bishops_bb[1] |
        board.rooks_bb[0]   | board.rooks_bb[1]   |
        board.queens_bb[0]  | board.queens_bb[1]  |
        board.kings_bb[0]   | board.kings_bb[1]);
    return pieces <= TB_LARGEST;
}

int32_t SyzygyProber::wdlToScore(WDL wdl, int ply) noexcept {
    switch (wdl) {
        case WDL::Win:         return TB_WIN_SCORE  - ply;
        case WDL::Loss:        return TB_LOSS_SCORE + ply;
        case WDL::BlessedLoss: return 0;
        case WDL::CursedWin:   return 0;
        case WDL::Draw:        return 0;
    }
    return 0;
}

uint64_t SyzygyProber::toPyrrhicBB(uint64_t bb) noexcept {
    return __builtin_bswap64(bb);
}

unsigned SyzygyProber::toPyrrhicEP(chess::Coords ep) noexcept {
    if (!ep.isValid()) return 0;
    return static_cast<unsigned>(63 ^ ep.index);
}

// HydraY convention: bb[0] = White, bb[1] = Black (colorToIndex(WHITE) == 0).
std::optional<WDL> SyzygyProber::probeWDL(const chess::Board& board) const {
    if (!loaded_) return std::nullopt;

    const uint64_t allW = toPyrrhicBB(
        board.pawns_bb[0] | board.knights_bb[0] | board.bishops_bb[0] |
        board.rooks_bb[0] | board.queens_bb[0]  | board.kings_bb[0]);
    const uint64_t allB = toPyrrhicBB(
        board.pawns_bb[1] | board.knights_bb[1] | board.bishops_bb[1] |
        board.rooks_bb[1] | board.queens_bb[1]  | board.kings_bb[1]);

    if (__builtin_popcountll(allW | allB) > TB_LARGEST) return std::nullopt;

    const bool turn = (board.getActiveColor() == chess::Board::WHITE);

    const unsigned result = tb_probe_wdl(
        allW, allB,
        toPyrrhicBB(board.kings_bb[0]   | board.kings_bb[1]),
        toPyrrhicBB(board.queens_bb[0]  | board.queens_bb[1]),
        toPyrrhicBB(board.rooks_bb[0]   | board.rooks_bb[1]),
        toPyrrhicBB(board.bishops_bb[0] | board.bishops_bb[1]),
        toPyrrhicBB(board.knights_bb[0] | board.knights_bb[1]),
        toPyrrhicBB(board.pawns_bb[0]   | board.pawns_bb[1]),
        toPyrrhicEP(board.getEnPassant()),
        turn);

    if (result == TB_RESULT_FAILED) return std::nullopt;
    return static_cast<WDL>(static_cast<int>(result) - 2);
}

std::vector<RootMove> SyzygyProber::probeRoot(const chess::Board& board) const {
    if (!loaded_) return {};

    const uint64_t allW = toPyrrhicBB(
        board.pawns_bb[0] | board.knights_bb[0] | board.bishops_bb[0] |
        board.rooks_bb[0] | board.queens_bb[0]  | board.kings_bb[0]);
    const uint64_t allB = toPyrrhicBB(
        board.pawns_bb[1] | board.knights_bb[1] | board.bishops_bb[1] |
        board.rooks_bb[1] | board.queens_bb[1]  | board.kings_bb[1]);

    if (__builtin_popcountll(allW | allB) > TB_LARGEST) return {};

    const bool turn = (board.getActiveColor() == chess::Board::WHITE);

    TbRootMoves results{};
    const int ok = tb_probe_root_dtz(
        allW, allB,
        toPyrrhicBB(board.kings_bb[0]   | board.kings_bb[1]),
        toPyrrhicBB(board.queens_bb[0]  | board.queens_bb[1]),
        toPyrrhicBB(board.rooks_bb[0]   | board.rooks_bb[1]),
        toPyrrhicBB(board.bishops_bb[0] | board.bishops_bb[1]),
        toPyrrhicBB(board.knights_bb[0] | board.knights_bb[1]),
        toPyrrhicBB(board.pawns_bb[0]   | board.pawns_bb[1]),
        static_cast<unsigned>(board.getHalfMoveClock()),
        toPyrrhicEP(board.getEnPassant()),
        turn,
        /*hasRepeated=*/false,
        &results);

    if (!ok) return {};

    std::vector<RootMove> out;
    out.reserve(results.size);
    for (unsigned i = 0; i < results.size; ++i) {
        const PyrrhicMove pm = results.moves[i].move;
        const uint8_t from = static_cast<uint8_t>(63 ^ PYRRHIC_MOVE_FROM(pm));
        const uint8_t to   = static_cast<uint8_t>(63 ^ PYRRHIC_MOVE_TO(pm));

        char promo = '\0';
        if      (PYRRHIC_MOVE_IS_QPROMO(pm)) promo = 'q';
        else if (PYRRHIC_MOVE_IS_RPROMO(pm)) promo = 'r';
        else if (PYRRHIC_MOVE_IS_BPROMO(pm)) promo = 'b';
        else if (PYRRHIC_MOVE_IS_NPROMO(pm)) promo = 'n';

        out.push_back({chess::Board::Move{chess::Coords{from}, chess::Coords{to}, promo},
                       results.moves[i].tbRank});
    }
    return out;
}

} // namespace syzygy
