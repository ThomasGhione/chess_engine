#ifndef PRINTS_HPP
#define PRINTS_HPP

#include <string>
#include "../board/board.hpp"

namespace print {

class Prints {
    public:
        static std::string getPrintableBoard(const std::string& FEN);
        static std::string getBasicBoard(const chess::Board& board);
        static std::string getBitBoard(const pieces::U64& bitboard);
};

}

#endif
