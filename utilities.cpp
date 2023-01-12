#include "chessengine.h"

namespace chess {

    std::string playerString(const unsigned char &player) noexcept {
        return (player == WHITE) ? "WHITE" : "BLACK";
    }

    int fromCharToInt(const char file) noexcept {
        return std::tolower(file) - 'a' + 1;
    }

}