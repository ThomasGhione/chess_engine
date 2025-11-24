#ifndef ENGINE_TT_HPP
#define ENGINE_TT_HPP

#include <cstdint>
#include <cstddef>

#include "../board/board.hpp"

namespace engine {

// Singola entry della transposition table.
// 16 byte: key(8) + depth(2) + score(4) + flag(1) + padding(1)
struct TTEntry {
    uint64_t key   = 0;   // Zobrist key completo
    uint16_t depth = 0;   // profondità alla quale è stato calcolato lo score
    int32_t  score = 0;   // valutazione (tipicamente centipawn o mate score)
    uint8_t  flag  = 0;   // EXACT / LOWERBOUND / UPPERBOUND
    uint8_t  padding = 0; // per allineare a 16 byte (facoltativo)

    enum Flag : uint8_t {
        EXACT      = 0,
        LOWERBOUND = 1,
        UPPERBOUND = 2
    };

    static constexpr std::size_t TABLE_SIZE = 1u << 20; // 2^20 entries (16 MB circa)

    TTEntry() = default;
    TTEntry(uint64_t k, uint16_t d, int32_t s, uint8_t f)
        : key(k), depth(d), score(s), flag(f) {}
};

// Transposition table globale: una sola tabella condivisa da tutto il motore.
inline TTEntry* globalTT() {
    static TTEntry table[TTEntry::TABLE_SIZE];
    return table;
}

// Zobrist: 16 pezzi x 64 caselle + side-to-move, castling, en-passant.
// Tutte le chiavi sono pre-calcolate a compile time.
namespace detail {

struct XorShift64 {
    uint64_t state;
    constexpr explicit XorShift64(uint64_t seed) : state(seed) {}
    constexpr uint64_t next() {
        uint64_t x = state;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state = x;
        return x * 0x2545F4914F6CDD1DULL;
    }
};

struct ZobristTables {
    uint64_t piece[16][64];
    uint64_t sideToMove;
    uint64_t castling[16];
    uint64_t enPassant[8];
};

constexpr ZobristTables makeZobristTables() {
    ZobristTables t{};
    XorShift64 rng(0x123456789ABCDEF0ULL);

    // Pezzi (16 indici: tipi+colori) x 64 caselle
    for (int p = 0; p < 16; ++p) {
        for (int sq = 0; sq < 64; ++sq) {
            t.piece[p][sq] = rng.next();
        }
    }

    // Side to move
    t.sideToMove = rng.next();

    // Castling 0..15 (bitmask KQkq)
    for (int i = 0; i < 16; ++i) {
        t.castling[i] = rng.next();
    }

    // En-passant file 0..7
    for (int f = 0; f < 8; ++f) {
        t.enPassant[f] = rng.next();
    }

    return t;
}

inline constexpr ZobristTables ZOBRIST = makeZobristTables();

} // namespace detail

inline uint64_t computeHashKey(const chess::Board& board) {
    uint64_t hashKey = 0ULL;

    // Pezzi su 64 caselle usando Board::get(sq)
    for (uint8_t sq = 0; sq < 64; ++sq) {
        uint8_t raw = board.get(sq);
        if (raw == chess::Board::EMPTY) continue;

        uint8_t type  = raw & chess::Board::MASK_PIECE_TYPE;
        uint8_t color = raw & chess::Board::MASK_COLOR;

        // Mappa (type,color) → indice 0..15
        uint8_t idx = 0;
        if (color == chess::Board::WHITE) {
            idx = type;            // 1..6 per i pezzi bianchi
        } else { // BLACK
            idx = 8 + type;        // 9..14 per i pezzi neri
        }

        hashKey ^= detail::ZOBRIST.piece[idx][sq];
    }

    // Side to move: XOR se tocca al nero
    if (board.getActiveColor() == chess::Board::BLACK) {
        hashKey ^= detail::ZOBRIST.sideToMove;
    }

    // Castling rights: vettore<bool> size 4 -> bitmask 0..15 (KQkq)
    uint8_t castlingMask = 0;
    if (board.getCastle(0)) castlingMask |= 1u << 0; // K
    if (board.getCastle(1)) castlingMask |= 1u << 1; // Q
    if (board.getCastle(2)) castlingMask |= 1u << 2; // k
    if (board.getCastle(3)) castlingMask |= 1u << 3; // q
    hashKey ^= detail::ZOBRIST.castling[castlingMask];

    // En-passant: usiamo il getter leggero per il side to move corrente.
    const uint8_t stm = board.getActiveColor();
    const chess::Coords epSquare = board.getEnPassant((stm == chess::Board::WHITE) ? 0 : 1);
    if (epSquare.file >= 0 && epSquare.file < 8 && epSquare.rank >= 0 && epSquare.rank < 8) {
        int file = epSquare.file;
        hashKey ^= detail::ZOBRIST.enPassant[file];
    }

    return hashKey;
}

// Store semplice: rimpiazza se stessa chiave o se il nuovo depth è >= del vecchio.
inline void storeTTEntry(TTEntry* ttTable,
                         uint64_t key,
                         uint16_t depth,
                         int32_t score,
                         uint8_t flag) {

    const std::size_t index = static_cast<std::size_t>(key) & (TTEntry::TABLE_SIZE - 1);
    TTEntry& entry = ttTable[index];

    if (entry.key == key || depth >= entry.depth) {
        entry.key   = key;
        entry.depth = depth;
        entry.score = score;
        entry.flag  = flag;
    }
}

// Probe minimale della TT.
inline bool probeTT(const TTEntry* ttTable,
                    uint64_t key,
                    uint16_t depth,
                    int32_t alpha,
                    int32_t beta,
                    int32_t& outScore) {

    const std::size_t index = static_cast<std::size_t>(key) & (TTEntry::TABLE_SIZE - 1);
    const TTEntry& entry = ttTable[index];

    if (entry.key != key) return false;      // chiave diversa o cella vuota
    if (entry.depth < depth) return false;   // profondità insufficiente

    switch (entry.flag) {
        case TTEntry::EXACT:
            outScore = entry.score;
            return true;
        case TTEntry::LOWERBOUND:
            if (entry.score >= beta) {
                outScore = entry.score;
                return true;
            }
            break;
        case TTEntry::UPPERBOUND:
            if (entry.score <= alpha) {
                outScore = entry.score;
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

} // namespace engine

#endif // ENGINE_TT_HPP