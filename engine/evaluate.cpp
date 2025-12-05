#include "engine.hpp"

namespace engine {
int64_t Engine::getMaterialDeltaFAST(const chess::Board& b) noexcept {
    int64_t delta = 0;

    // White pieces: color index 0
    // Black pieces: color index 1

    // Pawns
    {
        int w = __builtin_popcountll(b.pawns_bb[0]);
        int bl = __builtin_popcountll(b.pawns_bb[1]);
        delta += static_cast<int64_t>(w - bl) * PIECE_VALUES[chess::Board::PAWN];
    }

    // Knights
    {
        int w = __builtin_popcountll(b.knights_bb[0]);
        int bl = __builtin_popcountll(b.knights_bb[1]);
        delta += static_cast<int64_t>(w - bl) * PIECE_VALUES[chess::Board::KNIGHT];
    }

    // Bishops
    {
        int w = __builtin_popcountll(b.bishops_bb[0]);
        int bl = __builtin_popcountll(b.bishops_bb[1]);
        delta += static_cast<int64_t>(w - bl) * PIECE_VALUES[chess::Board::BISHOP];
    }

    // Rooks
    {
        int w = __builtin_popcountll(b.rooks_bb[0]);
        int bl = __builtin_popcountll(b.rooks_bb[1]);
        delta += static_cast<int64_t>(w - bl) * PIECE_VALUES[chess::Board::ROOK];
    }

    // Queens
    {
        int w = __builtin_popcountll(b.queens_bb[0]);
        int bl = __builtin_popcountll(b.queens_bb[1]);
        delta += static_cast<int64_t>(w - bl) * PIECE_VALUES[chess::Board::QUEEN];
    }

    // Kings
    {
        int w = __builtin_popcountll(b.kings_bb[0]);
        int bl = __builtin_popcountll(b.kings_bb[1]);
        delta += static_cast<int64_t>(w - bl) * PIECE_VALUES[chess::Board::KING];
    }

    return delta;
}

int64_t Engine::evaluateCheckmate(const chess::Board& board) {
    return (board.getActiveColor() == chess::Board::BLACK) ? POS_INF : NEG_INF;
}

int64_t Engine::evaluate(const chess::Board& board) {

    // 1) EVALUATION CHECKMATE
    if (board.isCheckmate(board.getActiveColor())) {
        return this->evaluateCheckmate(board);
    }

    int64_t eval = 0;

    int64_t materialDelta = getMaterialDeltaFAST(board);
    eval += materialDelta;

    
    // 2) IS THIS AN ENDGAME?
    // 2.1) counting major pieces...
    uint64_t allKnights = board.knights_bb[0] | board.knights_bb[1];
    uint64_t allBishops = board.bishops_bb[0] | board.bishops_bb[1];
    uint64_t allRooks   = board.rooks_bb[0]   | board.rooks_bb[1];
    uint64_t allQueens  = board.queens_bb[0]  | board.queens_bb[1];
    uint64_t minorMajors = allKnights | allBishops | allRooks | allQueens;
    int nonPawnNonKingPieces = __builtin_popcountll(minorMajors);
    // 2.2) finally decide if it's endgame
    bool isEndgame = (nonPawnNonKingPieces <= PHASE_FINAL_THRESHOLD);

    // 3) BONUS POSITION TABLE (bitboard-based per piece type)

    auto addPsqtFor = [&](uint64_t bbWhite, uint64_t bbBlack,
                           auto valueSelectorWhite) {
        
        // Nessun pezzo di questo tipo: niente da fare
        if (!(bbWhite | bbBlack)) return;

        // White pieces
        uint64_t bb = bbWhite;
        while (bb) {
            uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bb));
            bb &= (bb - 1);

            uint8_t idx = sq;
            int64_t posValue = valueSelectorWhite(idx);
            eval += posValue; // white adds
        }

        // Black pieces
        bb = bbBlack;
        while (bb) {
            uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bb));
            bb &= (bb - 1);

            uint8_t idx = mirrorIndex(sq);
            int64_t posValue = valueSelectorWhite(idx);
            eval -= posValue; // black subtracts
        }
    };

    // Pawns
    addPsqtFor(
        board.pawns_bb[0],
        board.pawns_bb[1],
        [&](uint8_t idx) {
            return isEndgame ? PAWN_END_GAME_VALUES_TABLE[idx]
                             : PAWN_VALUES_TABLE[idx];
        }
    );

    // Knights
    addPsqtFor(
        board.knights_bb[0],
        board.knights_bb[1],
        [&](uint8_t idx) { return KNIGHT_VALUES_TABLE[idx]; }
    );

    // Bishops
    addPsqtFor(
        board.bishops_bb[0],
        board.bishops_bb[1],
        [&](uint8_t idx) { return BISHOP_VALUES_TABLE[idx]; }
    );

    // Rooks
    addPsqtFor(
        board.rooks_bb[0],
        board.rooks_bb[1],
        [&](uint8_t idx) { return ROOK_VALUES_TABLE[idx]; }
    );

    // Queens
    addPsqtFor(
        board.queens_bb[0],
        board.queens_bb[1],
        [&](uint8_t idx) { return QUEEN_VALUES_TABLE[idx]; }
    );

    // Kings
    addPsqtFor(
        board.kings_bb[0],
        board.kings_bb[1],
        [&](uint8_t idx) {
            return isEndgame ? KING_END_GAME_VALUES_TABLE[idx]
                             : KING_MIDDLE_GAME_VALUES_TABLE[idx];
        }
    );

    // 4) Castling bonus in evaluation (not just move ordering)
    // Apply bonus only if king and rook squares match a castled configuration.
    auto addCastlingEvalBonus = [&](bool isWhite){
        const uint64_t kings = board.kings_bb[!isWhite];
        const uint64_t rooks = board.rooks_bb[!isWhite];
        if (!kings) return;
        const int kingSq = __builtin_ctzll(kings);

        // Indices for castled configurations
        // White: king g1 (6) with rook f1 (5) OR king c1 (2) with rook d1 (3)
        // Black: king g8 (62) with rook f8 (61) OR king c8 (58) with rook d8 (59)
        auto hasRookOn = [&](int idx){ return (rooks & (1ULL << idx)) != 0ULL; };

        bool castled = false;
        if (isWhite) {
            if (kingSq == 6 && hasRookOn(5)) castled = true;        // O-O
            else if (kingSq == 2 && hasRookOn(3)) castled = true;   // O-O-O
        } else {
            if (kingSq == 62 && hasRookOn(61)) castled = true;      // O-O
            else if (kingSq == 58 && hasRookOn(59)) castled = true; // O-O-O
        }

        if (castled) eval += isWhite ? CASTLING_BONUS : -CASTLING_BONUS;
    };

    addCastlingEvalBonus(true);
    addCastlingEvalBonus(false);
    
    // 5) King moves without castling should be penalized
    // (This requires tracking move history; skipping for now.)
    

    //int64_t whiteEval = 0, blackEval = 0;
    //eval += (whiteEval - blackEval);

    return eval;
}

bool Engine::isMate() {
    uint8_t toMove = this->board.getActiveColor();
    if (this->board.isCheckmate(toMove) || this->board.isStalemate(toMove)) {
        return true;
    }
    return false;
}


int64_t Engine::getMaterialDelta(const chess::Board& b) noexcept {

	static constexpr auto coefficientPiece = [](uint8_t piece) {
	    return -2 *static_cast<int64_t>(piece >> 3) + 1;
  };

  static constexpr auto pieceValue = [](uint8_t x) {
    return static_cast<int64_t>( (x * (-134220 +
      x * (304540 +
      x * (-240405 +
      x * (87775 +
      x * (-15075 +
      x * 985)))))) / 36.0);
  };

  int64_t delta = 0;
  static constexpr uint8_t MAX_INDEX = 64;
  //#pragma omp parallel for reduction(+:delta)
  for (uint8_t i = 0; i < MAX_INDEX; i++) {
    uint8_t piece = b.get(i);

    delta += coefficientPiece(piece & chess::Board::MASK_COLOR) * pieceValue(piece & chess::Board::MASK_PIECE_TYPE);
  }
  
  return delta;
}

}; // namespace engine
