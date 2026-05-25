#include "syzygy.hpp"

extern "C" {
#include "tbprobe.h"
}

#include <vector>

namespace syzygy {

bool SyzygyProber::load(const std::string& path) {
    loaded_ = tb_init(path.c_str());
    return loaded_;
}

int SyzygyProber::maxPieces() const noexcept {
    return TB_LARGEST;
}

uint64_t SyzygyProber::toPyrrhicBB(uint64_t bb) noexcept {
    return __builtin_bswap64(bb);
}

unsigned SyzygyProber::toPyrrhicEP(chess::Coords ep) noexcept {
    if (!ep.isValid()) return 0;
    // HydraY ep square is the square the pawn lands on (rank 2 or 5).
    // Pyrrhic ep: same square, converted. 63 ^ hydrY_sq gives pyrrhic sq.
    return static_cast<unsigned>(63 ^ ep.index);
}

std::optional<WDL> SyzygyProber::probeWDL(const chess::Board& board) const {
    if (!loaded_) return std::nullopt;

    const uint64_t allW = toPyrrhicBB(
        board.pawns_bb[1] | board.knights_bb[1] | board.bishops_bb[1] |
        board.rooks_bb[1] | board.queens_bb[1]  | board.kings_bb[1]);
    const uint64_t allB = toPyrrhicBB(
        board.pawns_bb[0] | board.knights_bb[0] | board.bishops_bb[0] |
        board.rooks_bb[0] | board.queens_bb[0]  | board.kings_bb[0]);

    const int pieces = __builtin_popcountll(allW | allB);
    if (pieces > TB_LARGEST) return std::nullopt;

    const bool turn = (board.getActiveColor() == chess::Board::WHITE);

    const unsigned result = tb_probe_wdl(
        allW, allB,
        toPyrrhicBB(board.kings_bb[1]   | board.kings_bb[0]),
        toPyrrhicBB(board.queens_bb[1]  | board.queens_bb[0]),
        toPyrrhicBB(board.rooks_bb[1]   | board.rooks_bb[0]),
        toPyrrhicBB(board.bishops_bb[1] | board.bishops_bb[0]),
        toPyrrhicBB(board.knights_bb[1] | board.knights_bb[0]),
        toPyrrhicBB(board.pawns_bb[1]   | board.pawns_bb[0]),
        toPyrrhicEP(board.getEnPassant()),
        turn);

    if (result == TB_RESULT_FAILED) return std::nullopt;

    // Map TB_LOSS..TB_WIN (0..4) to WDL enum (-2..+2).
    return static_cast<WDL>(static_cast<int>(result) - 2);
}

std::vector<RootMove> SyzygyProber::probeRoot(const chess::Board& board) const {
    if (!loaded_) return {};

    const uint64_t allW = toPyrrhicBB(
        board.pawns_bb[1] | board.knights_bb[1] | board.bishops_bb[1] |
        board.rooks_bb[1] | board.queens_bb[1]  | board.kings_bb[1]);
    const uint64_t allB = toPyrrhicBB(
        board.pawns_bb[0] | board.knights_bb[0] | board.bishops_bb[0] |
        board.rooks_bb[0] | board.queens_bb[0]  | board.kings_bb[0]);

    const int pieces = __builtin_popcountll(allW | allB);
    if (pieces > TB_LARGEST) return {};

    const bool turn = (board.getActiveColor() == chess::Board::WHITE);

    TbRootMoves results{};
    const int ok = tb_probe_root_dtz(
        allW, allB,
        toPyrrhicBB(board.kings_bb[1]   | board.kings_bb[0]),
        toPyrrhicBB(board.queens_bb[1]  | board.queens_bb[0]),
        toPyrrhicBB(board.rooks_bb[1]   | board.rooks_bb[0]),
        toPyrrhicBB(board.bishops_bb[1] | board.bishops_bb[0]),
        toPyrrhicBB(board.knights_bb[1] | board.knights_bb[0]),
        toPyrrhicBB(board.pawns_bb[1]   | board.pawns_bb[0]),
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

        // Decode Pyrrhic square (a1=0) → HydraY square (a8=0): 63 ^ sq.
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
