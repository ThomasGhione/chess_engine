namespace engine {

// Helper for LMR: check if the move is a killer move for the current ply
// OPTIMIZATION: manually unrolled the loop (2 iterations)
__attribute__((always_inline))
inline bool Engine::isKillerMove(const chess::Board::Move& m, const chess::Board::Move killerMoves[2][Engine::MAX_PLY], int ply) const noexcept {
    if (ply < 0 || ply >= Engine::MAX_PLY) [[unlikely]] return false;
    
    // Manual unroll: compare both killer moves without a loop
    const auto& km0 = killerMoves[0][ply];
    const auto& km1 = killerMoves[1][ply];
    
    return (m.from.index == km0.from.index && m.to.index == km0.to.index) ||
           (m.from.index == km1.from.index && m.to.index == km1.to.index);
}

// Helper function to check if a move is a pawn promotion candidate
// OPTIMIZATION: inline + noexcept + constexpr rank check
__attribute__((always_inline))
inline bool isPromotionMove(const chess::Board& board, const chess::Board::Move& move) noexcept {
    // Early exit: if not on rank 1 or 8, it cannot be a promotion
    const uint8_t toRank = move.to.rank();
    if (toRank != 0 && toRank != 7) return false;
    
    const uint8_t piece = board.get(move.from);
    const uint8_t pieceType = piece & chess::Board::MASK_PIECE_TYPE;
    
    if (pieceType != chess::Board::PAWN) return false;
    
    const uint8_t pieceColor = piece & chess::Board::MASK_COLOR;
    // White promotes at rank 7 (8th rank), Black promotes at rank 0 (1st rank)
    return toRank == chess::Board::promotionRank(pieceColor == chess::Board::WHITE);
}

// En-passant capture detection on a legal-move candidate.
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
// Returns true if the move was a promotion (for information)
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

} // namespace engine
