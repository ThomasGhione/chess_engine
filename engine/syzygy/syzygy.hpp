#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "../../board/board.hpp"

namespace syzygy {

// WDL result from tb_probe_wdl(), side-to-move relative.
enum class WDL : int8_t { Loss = -2, BlessedLoss = -1, Draw = 0, CursedWin = 1, Win = 2 };

struct RootMove {
    chess::Board::Move move;
    int32_t            tbRank; // higher = better; use to order root moves
};

class SyzygyProber {
public:
    // Load tablebases from a directory path. Returns false if no tables found.
    bool load(const std::string& path);
    bool isLoaded() const noexcept { return loaded_; }

    // Maximum pieces (including kings) supported by the loaded tables.
    int maxPieces() const noexcept;

    // WDL probe for use inside search (no 50-move rule consideration).
    // Returns nullopt when the position has too many pieces or probe fails.
    std::optional<WDL> probeWDL(const chess::Board& board) const;

    // Root probe: returns a ranked list of moves with DTZ info for the best
    // endgame play. Returns empty when probe fails or position not in TB.
    std::vector<RootMove> probeRoot(const chess::Board& board) const;

private:
    bool loaded_ = false;

    // Pack the board's bitboards into the Pyrrhic-convention form.
    // Pyrrhic squares: a1=0..h8=63; HydraY squares: a8=0..h1=63.
    // Conversion: pyrrhic_sq = 63 ^ hydrY_sq  →  bswap64 mirrors the board.
    static uint64_t toPyrrhicBB(uint64_t bb) noexcept;
    static unsigned toPyrrhicEP(chess::Coords ep) noexcept;
};

} // namespace syzygy
