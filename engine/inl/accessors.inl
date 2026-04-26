namespace engine {

inline bool Engine::isGameOver() const noexcept {
    return gameResult != ONGOING;
}

inline bool Engine::isMate() const noexcept {
    return gameResult == WHITE_WINS || gameResult == BLACK_WINS;
}

inline bool Engine::isStalemate() const noexcept {
    return gameResult == DRAW;
}

inline uint8_t Engine::getActiveColor() const noexcept {
    return board.getActiveColor();
}

} // namespace engine
