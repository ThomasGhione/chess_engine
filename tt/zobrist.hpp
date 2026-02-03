#ifndef TT_ZOBRIST_HPP
#define TT_ZOBRIST_HPP

#include <cstdint>
#include <cstddef>
#include <array>
#include "../board/board.hpp"

namespace zobrist {
    // RNG per compile-time generation
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

    // Costanti per dimensioni array
    constexpr std::size_t PIECE_TYPES = 16;    // 16 tipi di pezzo (0=empty, 1-6=white, 9-14=black)
    constexpr std::size_t SQUARES = 64;         // 64 caselle
    constexpr std::size_t CASTLING_STATES = 16; // 16 stati castling (bitmask KQkq: 0-15)
    constexpr std::size_t FILES = 8;            // 8 colonne per en-passant

    // Tabelle Zobrist con std::array
    struct Tables {
        std::array<std::array<uint64_t, SQUARES>, PIECE_TYPES> pieces;
        uint64_t sideToMove;
        std::array<uint64_t, CASTLING_STATES> castling;
        std::array<uint64_t, FILES> enPassant;
    };

    // Generazione compile-time
    constexpr Tables makeTables() {
        Tables t{};
        XorShift64 rng(0x123456789ABCDEF0ULL);

        // Pezzi: 16 tipi × 64 caselle
        for (std::size_t pieceType = 0; pieceType < PIECE_TYPES; ++pieceType) {
            for (std::size_t square = 0; square < SQUARES; ++square) {
                t.pieces[pieceType][square] = rng.next();
            }
        }

        // Side to move
        t.sideToMove = rng.next();

        // Castling: 16 stati possibili
        for (std::size_t i = 0; i < CASTLING_STATES; ++i) {
            t.castling[i] = rng.next();
        }

        // En-passant: 8 colonne
        for (std::size_t file = 0; file < FILES; ++file) {
            t.enPassant[file] = rng.next();
        }

        return t;
    }

    // Tabelle globali compile-time
    inline constexpr Tables TABLES = makeTables();

    // Helper per XOR pezzi da bitboard (più leggibile e riusabile)
    inline void xorPiecesFromBitboard(uint64_t& hashKey, uint64_t bitboard, std::size_t pieceIndex) {
        while (bitboard) {
            const uint8_t square = static_cast<uint8_t>(__builtin_ctzll(bitboard));
            bitboard &= (bitboard - 1);
            hashKey ^= TABLES.pieces[pieceIndex][square];
        }
    }

    // Calcolo chiave hash da Board
    inline uint64_t computeHashKey(const chess::Board& board) {
        uint64_t hashKey = 0ULL;

        // White pieces (piece index 1-6)
        xorPiecesFromBitboard(hashKey, board.pawns_bb[0],   1);
        xorPiecesFromBitboard(hashKey, board.knights_bb[0], 2);
        xorPiecesFromBitboard(hashKey, board.bishops_bb[0], 3);
        xorPiecesFromBitboard(hashKey, board.rooks_bb[0],   4);
        xorPiecesFromBitboard(hashKey, board.queens_bb[0],  5);
        xorPiecesFromBitboard(hashKey, board.kings_bb[0],   6);

        // Black pieces (piece index 9-14)
        xorPiecesFromBitboard(hashKey, board.pawns_bb[1],   9);
        xorPiecesFromBitboard(hashKey, board.knights_bb[1], 10);
        xorPiecesFromBitboard(hashKey, board.bishops_bb[1], 11);
        xorPiecesFromBitboard(hashKey, board.rooks_bb[1],   12);
        xorPiecesFromBitboard(hashKey, board.queens_bb[1],  13);
        xorPiecesFromBitboard(hashKey, board.kings_bb[1],  14);

        // Side to move (branchless: XOR if black to move)
        // FIX: avoid undefined behavior - cast to int64_t before negation
        const uint64_t stmMask = static_cast<uint64_t>(-static_cast<int64_t>(board.getActiveColor() == chess::Board::BLACK));
        hashKey ^= TABLES.sideToMove & stmMask;

        // Castling rights (0-15 bitmask)
        const uint8_t castlingMask =
            (board.getCastle(0) ? 1u : 0u) |
            (board.getCastle(1) ? 2u : 0u) |
            (board.getCastle(2) ? 4u : 0u) |
            (board.getCastle(3) ? 8u : 0u);
        hashKey ^= TABLES.castling[castlingMask];

        // En-passant (branchless: XOR if valid EP square)
        // FIX: check bounds BEFORE accessing file() to avoid out-of-bounds
        const chess::Coords epSquare = board.getEnPassant();
        const bool epValid = chess::Coords::isInBounds(epSquare);
        const uint64_t epMask = static_cast<uint64_t>(-static_cast<int64_t>(epValid));
        // Only access file() if epSquare is valid, otherwise use 0 (masked out anyway)
        const uint8_t epFile = epValid ? epSquare.file() : 0;
        hashKey ^= TABLES.enPassant[epFile] & epMask;

        return hashKey;
    }
}

#endif // TT_ZOBRIST_HPP
