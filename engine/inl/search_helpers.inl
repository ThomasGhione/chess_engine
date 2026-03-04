namespace engine {

__attribute__((always_inline))
inline bool Engine::isKillerMove(const chess::Board::Move& m, const chess::Board::Move killerMoves[2][Engine::MAX_PLY], int ply) const noexcept {
    if (ply < 0 || ply >= Engine::MAX_PLY) [[unlikely]] return false;
    
    // Manual unroll: compare both killer moves without a loop
    const auto& km0 = killerMoves[0][ply];
    const auto& km1 = killerMoves[1][ply];
    
    return (m.from.index == km0.from.index && m.to.index == km0.to.index) ||
           (m.from.index == km1.from.index && m.to.index == km1.to.index);
}

__attribute__((always_inline))
inline bool isPromotionMove(const chess::Board& board, const chess::Board::Move& move) noexcept {
    const uint8_t toRank = move.to.rank();
    if (toRank != 0 && toRank != 7) return false;
    
    const uint8_t piece = board.get(move.from);
    const uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
    
    if (pieceType != chess::Board::PAWN) return false;
    
    const uint8_t pieceColor = piece & chess::Board::MASK_COLOR;
    return toRank == chess::Board::promotionRank(pieceColor == chess::Board::WHITE);
}

__attribute__((always_inline))
inline bool isEnPassantCapture(const chess::Board& board, const chess::Board::Move& move) noexcept {
    const uint8_t fromPieceType = board.get(move.from) & chess::Board::MASK_PIECE_TYPE;
    if (fromPieceType != chess::Board::PAWN) return false;
    if (board.get(move.to) != chess::Board::EMPTY) return false;
    if (chess::Board::fileOf(move.from.index) == chess::Board::fileOf(move.to.index)) return false;

    const chess::Coords ep = board.getEnPassant();
    return chess::Coords::isInBounds(ep) && (move.to == ep);
}

// ============================================================================
// MOVE EXECUTION HELPERS - Eliminates doMove/undoMove duplication
// ============================================================================

// Execute a move with automatic promotion detection
__attribute__((always_inline))
inline bool doMoveWithPromotion(chess::Board& b, const chess::Board::Move& m, chess::Board::MoveState& state) noexcept {
    const bool isPromo = isPromotionMove(b, m);
    char promoChoice = '\0';
    if (isPromo) {
        const char promo = static_cast<char>(std::tolower(static_cast<unsigned char>(m.promotionPiece)));
        promoChoice = (promo == 'q' || promo == 'r' || promo == 'b' || promo == 'n') ? promo : 'q';
    }
    b.doMove(m, state, promoChoice);
    return isPromo;
}

__attribute__((always_inline))
inline bool Engine::shouldAbortSearch() const noexcept {
    return this->stopSearchRequested.load(std::memory_order_acquire)
        || this->ponderingStopRequested.load(std::memory_order_acquire);
}

__attribute__((always_inline))
inline int32_t Engine::clampToTTScore(int64_t value) noexcept {
    if (value > POS_INF) return static_cast<int32_t>(POS_INF);
    if (value < NEG_INF) return static_cast<int32_t>(NEG_INF);
    return static_cast<int32_t>(value);
}

__attribute__((always_inline))
inline void Engine::toTTProbeBounds(int64_t alpha, int64_t beta, int32_t& ttAlpha, int32_t& ttBeta) noexcept {
    const int64_t expandedAlpha = alpha - tt::TranspositionTable::ADJUSTMENT;
    const int64_t expandedBeta = beta + tt::TranspositionTable::ADJUSTMENT;
    ttAlpha = clampToTTScore(expandedAlpha);
    ttBeta = clampToTTScore(expandedBeta);
}

} // namespace engine
