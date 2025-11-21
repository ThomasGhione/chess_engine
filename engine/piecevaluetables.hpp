#ifndef ENGINE_PIECEVALUETABLES_HPP
#define ENGINE_PIECEVALUETABLES_HPP

/*
 *
 * source:
 * https://www.chessprogramming.org/Simplified_Evaluation_Function
 * white's boards, must mirror for black
 * 
 * another solution:
 * https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
 */

#include "engine.hpp"
#include "vector"

namespace engine {


    static constexpr std::array<int64_t, 64> PAWN_VALUES_TABLE {
          0,  0,  0,  0,  0,  0,  0,  0,
         50, 50, 50, 50, 50, 50, 50, 50,
         10, 10, 20, 30, 30, 20, 10, 10,
          5,  5, 10, 25, 25, 10,  5,  5,
          0,  0,  0, 20, 20,  0,  0,  0,
          5, -5,-10,  0,  0,-10, -5,  5,
          5, 10, 10,-20,-20, 10, 10,  5,
          0,  0,  0,  0,  0,  0,  0,  0
    };

    static constexpr std::array<int64_t, 64> PAWN_END_GAME_VALUES_TABLE {


    };

    static constexpr std::array<int64_t, 64> KNIGHT_VALUES_TABLE {
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    };

    static constexpr std::array<int64_t, 64> BISHOP_VALUES_TABLE {
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20,
    };

    static constexpr std::array<int64_t, 64> ROOK_VALUES_TABLE {
          0,  0,  0,  0,  0,  0,  0,  0,
          5, 10, 10, 10, 10, 10, 10,  5,
         -5,  0,  0,  0,  0,  0,  0, -5,
         -5,  0,  0,  0,  0,  0,  0, -5,
         -5,  0,  0,  0,  0,  0,  0, -5,
         -5,  0,  0,  0,  0,  0,  0, -5,
         -5,  0,  0,  0,  0,  0,  0, -5,
          0,  0,  0,  5,  5,  0,  0,  0
    };

    static constexpr std::array<int64_t, 64> QUEEN_VALUES_TABLE {
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
         -5,  0,  5,  5,  5,  5,  0, -5,
          0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    };

    static constexpr std::array<int64_t, 64> KING_MIDDLE_GAME_VALUES_TABLE {
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -20,-30,-30,-40,-40,-30,-30,-20,
        -10,-20,-20,-20,-20,-20,-20,-10,
         20, 20,  0,  0,  0,  0, 20, 20,
         20, 30, 10,  0,  0, 10, 30, 20
    };

    static constexpr std::array<int64_t, 64> KING_END_GAME_VALUES_TABLE {
        -50,-40,-30,-20,-20,-30,-40,-50,
        -30,-20,-10,  0,  0,-10,-20,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-30,  0,  0,  0,  0,-30,-30,
        -50,-30,-30,-30,-30,-30,-30,-50
    };

    constexpr std::array<int64_t, 64> mirrorTable(const std::array<int64_t, 64>& table) {
        std::array<int64_t, 64> mirroredTable{};
        for (size_t rank = 0; rank < 8; ++rank) {
            for (size_t file = 0; file < 8; ++file) {
                mirroredTable[(7 - rank) * 8 + file] = table[rank * 8 + file];
            }
        }
        return mirroredTable;
    }

    constexpr int64_t mirrorSquareIndex(int64_t index, const std::array<int64_t, 64>& table) {
        std::array<int64_t, 64> mirroredTable = mirrorTable(table);
        return mirroredTable[index];
    }

    constexpr int64_t mirrorIndex(int64_t index) {
        int64_t rank = index / 8;
        int64_t file = index % 8;
        int64_t mirroredRank = 7 - rank;
        return mirroredRank * 8 + file;
    }
}

#endif // ENGINE_PIECEVALUETABLES_HPP