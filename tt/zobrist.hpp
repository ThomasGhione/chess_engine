#pragma once

#include <bit>
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
    constexpr std::size_t PIECE_TYPES = 16;    // 16 encoded piece ids (0..15)
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
    inline void xorPiecesFromBB(uint64_t& hashKey, uint64_t bitboard, std::size_t pieceIndex) noexcept {
        while (bitboard) {
            const uint8_t square = static_cast<uint8_t>(std::countr_zero(bitboard));
            bitboard &= (bitboard - 1);
            hashKey ^= TABLES.pieces[pieceIndex][square];
        }
    }

    inline bool hasPseudoLegalEnPassantCapture(const chess::Board& board, const chess::Coords& epSquare) noexcept {
        if (!epSquare.isValid()) return false;
        const int sideToMove = chess::Board::colorToIndex(board.getActiveColor());
        const uint64_t candidatePawns = pieces::PAWN_ATTACKERS_TO[sideToMove][epSquare.index] & board.pawns_bb[sideToMove];
        return candidatePawns != 0ULL;
    }

    // Compute hash key from Board
    inline uint64_t computeHashKey(const chess::Board& board) noexcept {
        uint64_t hashKey = 0ULL;

        // White side bitboards are always index 0, black side bitboards index 1.
        // The encoded piece ids are derived from Board constants to stay robust
        // to internal color-bit layout changes.
        xorPiecesFromBB(hashKey, board.pawns_bb[0],   chess::Board::PAWN   | chess::Board::WHITE);
        xorPiecesFromBB(hashKey, board.knights_bb[0], chess::Board::KNIGHT | chess::Board::WHITE);
        xorPiecesFromBB(hashKey, board.bishops_bb[0], chess::Board::BISHOP | chess::Board::WHITE);
        xorPiecesFromBB(hashKey, board.rooks_bb[0],   chess::Board::ROOK   | chess::Board::WHITE);
        xorPiecesFromBB(hashKey, board.queens_bb[0],  chess::Board::QUEEN  | chess::Board::WHITE);
        xorPiecesFromBB(hashKey, board.kings_bb[0],   chess::Board::KING   | chess::Board::WHITE);

        xorPiecesFromBB(hashKey, board.pawns_bb[1],   chess::Board::PAWN   | chess::Board::BLACK);
        xorPiecesFromBB(hashKey, board.knights_bb[1], chess::Board::KNIGHT | chess::Board::BLACK);
        xorPiecesFromBB(hashKey, board.bishops_bb[1], chess::Board::BISHOP | chess::Board::BLACK);
        xorPiecesFromBB(hashKey, board.rooks_bb[1],   chess::Board::ROOK   | chess::Board::BLACK);
        xorPiecesFromBB(hashKey, board.queens_bb[1],  chess::Board::QUEEN  | chess::Board::BLACK);
        xorPiecesFromBB(hashKey, board.kings_bb[1],   chess::Board::KING   | chess::Board::BLACK);

        const uint64_t stmMask = (board.getActiveColor() == chess::Board::BLACK) ? ~0ULL : 0ULL;
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
        const uint64_t epMask = epHashable ? ~0ULL : 0ULL;
        const uint8_t epFile = epHashable ? epSquare.file() : 0;
        hashKey ^= TABLES.enPassant[epFile] & epMask;

        return hashKey;
    }
}
