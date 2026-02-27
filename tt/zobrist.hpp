#ifndef TT_ZOBRIST_HPP
#define TT_ZOBRIST_HPP

#include <cstdint>
#include <cstddef>
#include <array>
#include "../board/board.hpp"

namespace zobrist {
    // RNG for compile-time generation
    struct XorShift64 {
        uint64_t state;
        constexpr explicit XorShift64(uint64_t seed) noexcept : state(seed) {}
        constexpr uint64_t next() noexcept {
            uint64_t x = state;
            x ^= x >> 12;
            x ^= x << 25;
            x ^= x >> 27;
            state = x;
            return x * 0x2545F4914F6CDD1DULL;
        }
    };

    // Array size constants
    constexpr std::size_t PIECE_TYPES = 16;    // 16 piece types (0=empty, 1-6=white, 9-14=black)
    constexpr std::size_t SQUARES = 64;         // 64 squares
    constexpr std::size_t CASTLING_STATES = 16; // 16 castling states (bitmask KQkq: 0-15)
    constexpr std::size_t FILES = 8;            // 8 files for en-passant

    // Zobrist tables using std::array
    struct Tables {
        std::array<std::array<uint64_t, SQUARES>, PIECE_TYPES> pieces;
        uint64_t sideToMove;
        std::array<uint64_t, CASTLING_STATES> castling;
        std::array<uint64_t, FILES> enPassant;
    };

    // Compile-time generation
    constexpr Tables makeTables() noexcept {
        Tables t{};
        XorShift64 rng(0x123456789ABCDEF0ULL);

        // Pieces: 16 types × 64 squares
        for (std::size_t pieceType = 0; pieceType < PIECE_TYPES; ++pieceType) {
            for (std::size_t square = 0; square < SQUARES; ++square) {
                t.pieces[pieceType][square] = rng.next();
            }
        }

        // Side to move
        t.sideToMove = rng.next();

        // Castling: 16 possible states
        for (std::size_t i = 0; i < CASTLING_STATES; ++i) {
            t.castling[i] = rng.next();
        }

        // En-passant: 8 files
        for (std::size_t file = 0; file < FILES; ++file) {
            t.enPassant[file] = rng.next();
        }

        return t;
    }

    // Global compile-time tables
    inline constexpr Tables TABLES = makeTables();

    // Helper to XOR pieces from bitboards (more readable and reusable)
    inline void xorPiecesFromBitboard(uint64_t& hashKey, uint64_t bitboard, std::size_t pieceIndex) noexcept {
        while (bitboard) {
            const uint8_t square = static_cast<uint8_t>(__builtin_ctzll(bitboard));
            bitboard &= (bitboard - 1);
            hashKey ^= TABLES.pieces[pieceIndex][square];
        }
    }

    inline bool hasPseudoLegalEnPassantCapture(const chess::Board& board, const chess::Coords& epSquare) noexcept {
        if (!chess::Coords::isInBounds(epSquare)) return false;
        const int sideToMove = chess::Board::colorToIndex(board.getActiveColor());
        const uint64_t candidatePawns = pieces::PAWN_ATTACKERS_TO[sideToMove][epSquare.index] & board.pawns_bb[sideToMove];
        return candidatePawns != 0ULL;
    }

    // Compute hash key from Board
    inline uint64_t computeHashKey(const chess::Board& board) noexcept {
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

        const uint64_t stmMask = static_cast<uint64_t>(-static_cast<int64_t>(board.getActiveColor() == chess::Board::BLACK));
        hashKey ^= TABLES.sideToMove & stmMask;

        // Castling rights (0-15 bitmask)
        const uint8_t castlingMask =
            (board.getCastle(0) ? 1u : 0u) |
            (board.getCastle(1) ? 2u : 0u) |
            (board.getCastle(2) ? 4u : 0u) |
            (board.getCastle(3) ? 8u : 0u);
        hashKey ^= TABLES.castling[castlingMask];

        // En-passant hashing (standard):
        // include EP file only if side to move has at least one pawn that can
        // pseudo-legally capture on the EP square.
        const chess::Coords epSquare = board.getEnPassant();
        const bool epHashable = hasPseudoLegalEnPassantCapture(board, epSquare);
        const uint64_t epMask = static_cast<uint64_t>(-static_cast<int64_t>(epHashable));
        const uint8_t epFile = epHashable ? epSquare.file() : 0;
        hashKey ^= TABLES.enPassant[epFile] & epMask;

        return hashKey;
    }
}

#endif // TT_ZOBRIST_HPP
