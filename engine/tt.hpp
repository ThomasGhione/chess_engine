#ifndef ENGINE_TT_HPP
#define ENGINE_TT_HPP

#include <cstdint>
#include <cstddef>

#include "../board/board.hpp"

namespace engine {

// Ottimizzazione: entry compatta da 12 byte (più cache-friendly)
// key16(2) + score(2) + depth(1) + age(1) + flag(1) + padding(1) = 8 byte minimo
// Usiamo key32 per ridurre collisioni: key32(4) + score(2) + depth(1) + age(1) + flag(1) + pad(3) = 12 byte
struct TTEntry {
    uint64_t key = 0;   // 64 bit della Zobrist key 
    int16_t  score = 0;   // score in centipawns (range ±32k sufficiente con mate detection)
    uint8_t  depth = 0;   // profondità (0-255 sufficiente)
    uint8_t  age   = 0;   // generation/age per replacement policy
    uint8_t  flag  = 0;   // EXACT / LOWERBOUND / UPPERBOUND
    uint8_t  padding[3] = {0, 0, 0}; // align a 16 byte

    enum Flag : uint8_t {
        EXACT      = 0,
        LOWERBOUND = 1,
        UPPERBOUND = 2
    };

    // Bucket-based TT: 4 entries per bucket per ridurre collisioni (cache line = 64 byte, bucket = 48 byte)
    static constexpr std::size_t BUCKET_COUNT = 1u << 22; // 2^22 buckets = 16M entries (~192 MB)
    static constexpr std::size_t ENTRIES_PER_BUCKET = 4;
    static constexpr std::size_t TABLE_SIZE = BUCKET_COUNT * ENTRIES_PER_BUCKET;
    
    static constexpr int32_t ADJUSTMENT = 50; 

    TTEntry() = default;
    TTEntry(uint64_t k, uint8_t d, int16_t s, uint8_t f, uint8_t a)
        : key(k), score(s), depth(d), age(a), flag(f) {}
};

// Transposition table globale + age/generation counter
struct TTGlobal {
    TTEntry table[TTEntry::TABLE_SIZE];
    uint8_t generation = 0;
};

inline TTGlobal& globalTTData() {
    static TTGlobal data;
    return data;
}

inline TTEntry* globalTT() {
    return globalTTData().table;
}

inline void incrementTTGeneration() {
    ++globalTTData().generation;
}

inline uint8_t getCurrentGeneration() {
    return globalTTData().generation;
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

    // Pezzi usando direttamente le bitboard per tipo/colore
    auto xorPiecesFromBB = [&](uint64_t bb, uint8_t idx) {
        while (bb) {
            const uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bb));
            bb &= (bb - 1);
            hashKey ^= detail::ZOBRIST.piece[idx][sq];
        }
    };

    // White pieces (color index 0) - indici 1..6
    xorPiecesFromBB(board.pawns_bb[0],   1);
    xorPiecesFromBB(board.knights_bb[0], 2);
    xorPiecesFromBB(board.bishops_bb[0], 3);
    xorPiecesFromBB(board.rooks_bb[0],   4);
    xorPiecesFromBB(board.queens_bb[0],  5);
    xorPiecesFromBB(board.kings_bb[0],   6);

    // Black pieces (color index 1) - indici 9..14
    xorPiecesFromBB(board.pawns_bb[1],   9);
    xorPiecesFromBB(board.knights_bb[1], 10);
    xorPiecesFromBB(board.bishops_bb[1], 11);
    xorPiecesFromBB(board.rooks_bb[1],   12);
    xorPiecesFromBB(board.queens_bb[1],  13);
    xorPiecesFromBB(board.kings_bb[1],   14);

    // Side to move: XOR se tocca al nero (branchless)
    const uint64_t stmMask = static_cast<uint64_t>(-(board.getActiveColor() == chess::Board::BLACK));
    hashKey ^= detail::ZOBRIST.sideToMove & stmMask;

    // Castling rights: usa direttamente il bitmask castle (0-15)
    const uint8_t castlingMask = 
        (board.getCastle(0) ? 1u : 0u) |
        (board.getCastle(1) ? 2u : 0u) |
        (board.getCastle(2) ? 4u : 0u) |
        (board.getCastle(3) ? 8u : 0u);
    hashKey ^= detail::ZOBRIST.castling[castlingMask];

    // En-passant: usa il getter leggero per il side to move corrente
    const chess::Coords epSquare = board.getEnPassant((board.getActiveColor() == chess::Board::WHITE) ? 0 : 1);
    const uint64_t epMask = static_cast<uint64_t>(-static_cast<int64_t>(chess::Coords::isInBounds(epSquare)));
    hashKey ^= detail::ZOBRIST.enPassant[epSquare.file] & epMask;

    return hashKey;
}

// Prefetch TT bucket per ridurre cache miss (chiamare prima di probe/store)
inline void prefetchTT(uint64_t key) {
    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (TTEntry::BUCKET_COUNT - 1);
    const TTEntry* bucket = &globalTT()[bucketIndex * TTEntry::ENTRIES_PER_BUCKET];
    __builtin_prefetch(bucket, 0, 3); // Read prefetch, high temporal locality
}

// Store con replacement policy: depth-preferred + age-based
inline void storeTTEntry(TTEntry* ttTable,
                         uint64_t key,
                         uint8_t depth,
                         int16_t score,
                         uint8_t flag) {

    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (TTEntry::BUCKET_COUNT - 1);
    TTEntry* bucket = &ttTable[bucketIndex * TTEntry::ENTRIES_PER_BUCKET];
    
    const uint64_t key32 = static_cast<uint64_t>(key);
    const uint8_t generation = getCurrentGeneration();

    TTEntry* replaceEntry = &bucket[0];
    int bestReplaceScore = -1000000;

    // Cerca entry esistente o la migliore da sostituire
    for (std::size_t i = 0; i < TTEntry::ENTRIES_PER_BUCKET; ++i) {
        TTEntry& entry = bucket[i];
        
        // Se stessa key, aggiorna sempre (depth-preferred)
        if (entry.key == key) {
            if (depth >= entry.depth || flag == TTEntry::EXACT) {
                entry.depth = depth;
                entry.score = score;
                entry.flag  = flag;
                entry.age   = generation;
            }
            return;
        }

        // Calcola replacement score: preferisci entry vecchie o con depth bassa
        const int ageDiff = static_cast<int>(generation - entry.age) & 0xFF;
        const int replaceScore = (ageDiff * 256) - static_cast<int>(entry.depth) * 4;
        
        if (replaceScore > bestReplaceScore) {
            bestReplaceScore = replaceScore;
            replaceEntry = &entry;
        }
    }

    // Sostituisci la migliore entry trovata
    replaceEntry->key = key;
    replaceEntry->depth = depth;
    replaceEntry->score = score;
    replaceEntry->flag  = flag;
    replaceEntry->age   = generation;
}

// Probe ottimizzato con bucket search
inline bool probeTT(const TTEntry* ttTable,
                    uint64_t key,
                    uint8_t depth,
                    int16_t alpha,
                    int16_t beta,
                    int16_t& outScore) {

    const std::size_t bucketIndex = static_cast<std::size_t>(key) & (TTEntry::BUCKET_COUNT - 1);
    const TTEntry* bucket = &ttTable[bucketIndex * TTEntry::ENTRIES_PER_BUCKET];
    
    const uint64_t key64 = static_cast<uint64_t>(key);

    // Cerca in tutte le entry del bucket
    for (std::size_t i = 0; i < TTEntry::ENTRIES_PER_BUCKET; ++i) {
        const TTEntry& entry = bucket[i];
        
        if (entry.key != key) continue;      // chiave diversa
        if (entry.depth < depth) continue;        // profondità insufficiente

        const int16_t score = entry.score;
        
        switch (entry.flag) {
            case TTEntry::EXACT:
                outScore = score;
                return true;
            case TTEntry::LOWERBOUND:
                if (score >= beta) {
                    outScore = score;
                    return true;
                }
                break;
            case TTEntry::UPPERBOUND:
                if (score <= alpha) {
                    outScore = score;
                    return true;
                }
                break;
        }
        
        // Entry trovata ma non utilizzabile per cutoff
        return false;
    }
    
    return false;
}

} // namespace engine

#endif // ENGINE_TT_HPP