#include "opening_book.hpp"
#include "polyglot_keys.hpp"

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <random>

namespace opening {

// ── file loading ───────────────────────────────────────────────────────────

bool OpeningBook::load(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (size <= 0 || size % 16 != 0) {
        std::fclose(f);
        return false;
    }

    const size_t n = static_cast<size_t>(size) / 16;
    entries_.resize(n);

    // Each on-disk entry: key(8 BE) + move(2 BE) + weight(2 BE) + learn(4 BE)
    for (size_t i = 0; i < n; ++i) {
        uint8_t buf[16];
        if (std::fread(buf, 16, 1, f) != 1) {
            std::fclose(f);
            entries_.clear();
            return false;
        }
        uint64_t key;    std::memcpy(&key,    buf,      sizeof(key));
        uint16_t move;   std::memcpy(&move,   buf + 8,  sizeof(move));
        uint16_t weight; std::memcpy(&weight, buf + 10, sizeof(weight));
        entries_[i].key    = std::byteswap(key);
        entries_[i].move   = std::byteswap(move);
        entries_[i].weight = std::byteswap(weight);
        // buf[12..15] = learn, ignored
    }

    std::fclose(f);
    return true;
}

// ── hash computation ────────────────────────────────────────────────────────

// HydraY square index (a8=0, h8=7, ..., a1=56, h1=63) → cutechess internal sq
// rank = idx >> 3  (0=top row=rank8, 7=bottom row=rank1)
// internal_sq = (2 + rank) * 10 + 1 + file
static constexpr uint32_t toCutechessInternal(uint8_t idx) noexcept {
    return (2u + (idx >> 3)) * 10u + 1u + (idx & 7u);
}

// XOR in all squares of a bitboard for one (side, pieceType) combination.
static uint64_t xorBB(uint64_t bb, int side, int ptype) noexcept {
    uint64_t h = 0;
    while (bb) {
        const uint8_t sq = static_cast<uint8_t>(std::countr_zero(bb));
        bb &= bb - 1;
        const uint32_t isq = toCutechessInternal(sq);
        h ^= POLYGLOT_KEYS[361 + 120 * 7 * side + ptype * 120 + isq];
    }
    return h;
}

uint64_t OpeningBook::polyglotKey(const chess::Board& board) noexcept {
    uint64_t key = 0;

    // Pieces: side 0=White, 1=Black; types 1=Pawn..6=King
    key ^= xorBB(board.pawns_bb[0],   0, 1); // white pawns
    key ^= xorBB(board.knights_bb[0], 0, 2);
    key ^= xorBB(board.bishops_bb[0], 0, 3);
    key ^= xorBB(board.rooks_bb[0],   0, 4);
    key ^= xorBB(board.queens_bb[0],  0, 5);
    key ^= xorBB(board.kings_bb[0],   0, 6);

    key ^= xorBB(board.pawns_bb[1],   1, 1); // black pawns
    key ^= xorBB(board.knights_bb[1], 1, 2);
    key ^= xorBB(board.bishops_bb[1], 1, 3);
    key ^= xorBB(board.rooks_bb[1],   1, 4);
    key ^= xorBB(board.queens_bb[1],  1, 5);
    key ^= xorBB(board.kings_bb[1],   1, 6);

    // Castling rights (uses rook's original square as the key index)
    // WK=h1(63), WQ=a1(56), BK=h8(7), BQ=a8(0)
    if (board.getCastle(chess::Board::WHITE_KINGSIDE))
        key ^= POLYGLOT_KEYS[121 + toCutechessInternal(63)];
    if (board.getCastle(chess::Board::WHITE_QUEENSIDE))
        key ^= POLYGLOT_KEYS[121 + toCutechessInternal(56)];
    if (board.getCastle(chess::Board::BLACK_KINGSIDE))
        key ^= POLYGLOT_KEYS[121 + 120 + toCutechessInternal(7)];
    if (board.getCastle(chess::Board::BLACK_QUEENSIDE))
        key ^= POLYGLOT_KEYS[121 + 120 + toCutechessInternal(0)];

    // En passant: include only when the side to move has a pawn that can capture.
    const chess::Square ep = board.getEnPassant();
    if (chess::isValidSquare(ep)) {
        const int stm = chess::Board::colorToIndex(board.getActiveColor());
        const uint64_t candidates =
            pieces::PAWN_ATTACKERS_TO[stm][ep] & board.pawns_bb[stm];
        if (candidates != 0ULL)
            key ^= POLYGLOT_KEYS[1 + toCutechessInternal(ep)];
    }

    // Side to move: XOR key when White is to move (cutechess convention,
    // equivalent to polyglot's XOR-for-Black because the key is shared).
    if (board.getActiveColor() == chess::Board::WHITE)
        key ^= POLYGLOT_KEYS[0];

    return key;
}

// ── move decoding ───────────────────────────────────────────────────────────

chess::Move OpeningBook::decodeMove(uint16_t pgMove) noexcept {
    const uint8_t to_file   = pgMove & 7;
    const uint8_t to_rank   = (pgMove >> 3) & 7;   // polyglot rank: 0=rank1
    const uint8_t from_file = (pgMove >> 6) & 7;
    const uint8_t from_rank = (pgMove >> 9) & 7;
    const uint8_t promo     = (pgMove >> 12) & 7;  // 0=none,1=N,2=B,3=R,4=Q

    // Convert polyglot square → HydraY index (rank 0=rank8, 7=rank1)
    const uint8_t from_idx = static_cast<uint8_t>((7u - from_rank) * 8u + from_file);
    uint8_t       to_idx   = static_cast<uint8_t>((7u - to_rank)   * 8u + to_file);

    // Castling: polyglot encodes king→rook-original-square.
    // Detect: king on e-file, same rank, moving to a- or h-file.
    // Remap to the king's actual destination (g or c file).
    if (from_file == 4 && from_rank == to_rank &&
        (to_file == 0 || to_file == 7)) {
        const uint8_t kingToFile = (to_file == 7) ? 6u : 2u; // g or c file
        to_idx = static_cast<uint8_t>((7u - to_rank) * 8u + kingToFile);
    }

    // Promotion char: '\0' = none, 'n','b','r','q'
    static constexpr char PROMO_CHARS[5] = {'\0', 'n', 'b', 'r', 'q'};
    const char promo_char = (promo < 5) ? PROMO_CHARS[promo] : '\0';

    return { from_idx, to_idx, promo_char };
}

// ── book probe ──────────────────────────────────────────────────────────────

std::span<const OpeningBook::Entry> OpeningBook::findEntries(uint64_t key) const noexcept {
    const auto begin = entries_.begin();
    const auto end   = entries_.end();

    // Lower bound
    auto lo = std::lower_bound(begin, end, key,
        [](const Entry& e, uint64_t k) { return e.key < k; });
    if (lo == end || lo->key != key)
        return {};

    // Upper bound
    auto hi = std::upper_bound(lo, end, key,
        [](uint64_t k, const Entry& e) { return k < e.key; });

    return std::span<const Entry>{lo, hi};
}

std::optional<chess::Move> OpeningBook::probe(const chess::Board& board) const {
    if (entries_.empty()) return std::nullopt;

    const uint64_t key      = polyglotKey(board);
    const auto     candidates = findEntries(key);
    if (candidates.empty()) return std::nullopt;

    // Weighted random selection: pick proportionally to weight.
    const uint32_t total = std::accumulate(
        candidates.begin(), candidates.end(), 0u,
        [](uint32_t acc, const Entry& e) { return acc + e.weight; });

    if (total == 0) return std::nullopt;

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist(0, total - 1);
    uint32_t pick = dist(rng);

    for (const auto& e : candidates) {
        if (pick < e.weight) return decodeMove(e.move);
        pick -= e.weight;
    }

    // Fallback: return highest-weight entry (shouldn't normally reach here)
    const auto& best = *std::max_element(
        candidates.begin(), candidates.end(),
        [](const Entry& a, const Entry& b) { return a.weight < b.weight; });
    return decodeMove(best.move);
}

} // namespace opening
