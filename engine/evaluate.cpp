#include "engine.hpp"

namespace engine {
    
inline int64_t Engine::getMaterialDeltaFAST(const chess::Board& b) noexcept {
    return static_cast<int64_t>(
          (__builtin_popcountll(b.pawns_bb[0])   - __builtin_popcountll(b.pawns_bb[1]))   * PIECE_VALUES[chess::Board::PAWN]
        + (__builtin_popcountll(b.knights_bb[0]) - __builtin_popcountll(b.knights_bb[1])) * PIECE_VALUES[chess::Board::KNIGHT]
        + (__builtin_popcountll(b.bishops_bb[0]) - __builtin_popcountll(b.bishops_bb[1])) * PIECE_VALUES[chess::Board::BISHOP]
        + (__builtin_popcountll(b.rooks_bb[0])   - __builtin_popcountll(b.rooks_bb[1]))   * PIECE_VALUES[chess::Board::ROOK]
        + (__builtin_popcountll(b.queens_bb[0])  - __builtin_popcountll(b.queens_bb[1]))  * PIECE_VALUES[chess::Board::QUEEN]
        + (__builtin_popcountll(b.kings_bb[0])   - __builtin_popcountll(b.kings_bb[1]))   * PIECE_VALUES[chess::Board::KING]);
}


int64_t Engine::evaluateCheckmate(const chess::Board& board) noexcept {
    return (board.getActiveColor() == chess::Board::BLACK) ? POS_INF : NEG_INF;
}

inline void addPsqt(uint64_t bbWhite, uint64_t bbBlack, const int64_t* table, int64_t& eval) noexcept {
    // White pieces: use index as-is
    while (bbWhite) {
        uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bbWhite));
        bbWhite &= (bbWhite - 1);
        eval += table[sq];
    }
    // Black pieces: mirror index vertically
    while (bbBlack) {
        uint8_t sq = static_cast<uint8_t>(__builtin_ctzll(bbBlack));
        bbBlack &= (bbBlack - 1);
        uint8_t idx = mirrorIndex(sq);
        eval -= table[idx];
    }
}

int64_t Engine::evaluate(const chess::Board& board) noexcept {
    if (board.isCheckmate(board.getActiveColor())) [[unlikely]] {
        return evaluateCheckmate(board);
    }

    int64_t eval = getMaterialDeltaFAST(board);

    // Endgame flag
    int nonPawnMajors = __builtin_popcountll(board.knights_bb[0] | board.knights_bb[1] |
                                             board.bishops_bb[0] | board.bishops_bb[1] |
                                             board.rooks_bb[0]   | board.rooks_bb[1]   |
                                             board.queens_bb[0]  | board.queens_bb[1]);
    bool isEndgame = (nonPawnMajors <= PHASE_FINAL_THRESHOLD);

    // Pawns
    addPsqt(board.pawns_bb[0], board.pawns_bb[1], (isEndgame ? PAWN_END_GAME_VALUES_TABLE : PAWN_VALUES_TABLE).data(), eval);
    // Knights, Bishops, Rooks, Queens, Kings
    addPsqt(board.knights_bb[0], board.knights_bb[1], KNIGHT_VALUES_TABLE.data(), eval);
    addPsqt(board.bishops_bb[0], board.bishops_bb[1], BISHOP_VALUES_TABLE.data(), eval);
    addPsqt(board.rooks_bb[0],   board.rooks_bb[1],   ROOK_VALUES_TABLE.data(), eval);
    addPsqt(board.queens_bb[0],  board.queens_bb[1],  QUEEN_VALUES_TABLE.data(), eval);
    addPsqt(board.kings_bb[0],   board.kings_bb[1],   (isEndgame ? KING_END_GAME_VALUES_TABLE : KING_MIDDLE_GAME_VALUES_TABLE).data(), eval);

    // Castling bonus (bitmask)
    if ((board.kings_bb[0] & ((1ULL << 62)|(1ULL << 58))) && 
        (board.rooks_bb[0] & ((1ULL << 61)|(1ULL << 59)))) eval += CASTLING_BONUS;
    if ((board.kings_bb[1] & ((1ULL << 6)|(1ULL << 2))) && 
        (board.rooks_bb[1] & ((1ULL << 5)|(1ULL << 3)))) eval -= CASTLING_BONUS;

    return eval;
}

bool Engine::isMate() noexcept{
    uint8_t toMove = this->board.getActiveColor();
    if (this->board.isCheckmate(toMove) || this->board.isStalemate(toMove)) {
        return true;
    }
    return false;
}


int64_t Engine::getMaterialDelta(const chess::Board& b) noexcept {

	static constexpr auto coefficientPiece = [](uint8_t piece) {
	    return -2 * static_cast<int64_t>(piece >> 3) + 1;
  };

  static constexpr auto pieceValue = [](uint8_t x) {
    const int64_t x64 = static_cast<int64_t>(x);  // Cast PRIMA delle moltiplicazioni
    return (x64 * (-134220 +
      x64 * (304540 +
      x64 * (-240405 +
      x64 * (87775 +
      x64 * (-15075 +
      x64 * 985)))))) / 36;
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
