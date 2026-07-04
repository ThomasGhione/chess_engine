#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "../../board/board.hpp"

namespace opening {

// Reader for Polyglot opening books (.bin format).
//
// Binary format (big-endian, 16 bytes per entry, sorted by key):
//   key    uint64  Polyglot Zobrist hash of the position
//   move   uint16  encoded move (bits 0-2 to_file, 3-5 to_rank, 6-8 from_file,
//                  9-11 from_rank, 12-14 promotion; all ranks 0=rank1..7=rank8)
//   weight uint16  relative frequency (higher = preferred)
//   learn  uint32  learning data (ignored)
//
// Castling moves in polyglot are encoded as king→rook-original-square;
// probe() translates them to king→king-destination-square for the engine.
class OpeningBook {
public:
    bool load(const std::string& path);
    bool isLoaded() const noexcept { return !entries_.empty(); }

    // Returns a book move for this position (weighted-random selection among
    // all candidates), or nullopt if the position is not in the book.
    std::optional<chess::Move> probe(const chess::Board& board) const;

private:
    struct Entry {
        uint64_t key;
        uint16_t move;
        uint16_t weight;
    };

    std::vector<Entry> entries_;

    // Compute the Polyglot Zobrist hash of the position.
    static uint64_t polyglotKey(const chess::Board& board) noexcept;

    // Decode a 16-bit polyglot move word to a Move.
    static chess::Move decodeMove(uint16_t pgMove) noexcept;

    // Binary-search for all entries matching key (file is sorted by key).
    std::span<const Entry> findEntries(uint64_t key) const noexcept;
};

} // namespace opening
