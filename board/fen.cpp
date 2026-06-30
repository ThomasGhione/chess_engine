#include "board.hpp"
#include <algorithm>
#include <charconv>
#include <sstream>

namespace chess {

//FIXME Evitare namespace anonimo
namespace {

constexpr std::array<char, 8> PIECE_TYPE_TO_CHAR = {
    '.', 'P', 'N', 'B', 'R', 'Q', 'K', '?'
};

} // namespace

bool Board::parseBoardSection(const std::string& boardSection, std::array<uint32_t, 8>& parsedBoard) {
    int rank = 7, file = 0;
    for (char c : boardSection) {
	//FIXME Creare funzione heleper per gestire questa logica
        if (c == '/') { --rank; file = 0; }
        else if (std::isdigit(static_cast<unsigned char>(c))) file += c - '0';
        else {
            if (rank < 0 || file > 7) return false;
            uint8_t p = Board::CHAR_TO_PIECE_TYPE[static_cast<uint8_t>(c)];
            if (p == EMPTY) return false;
            parsedBoard[rank] |= static_cast<uint32_t>(p) << (file++ * 4);
        }
    }
    return rank == 0 && file == 8;
}

uint8_t Board::parseActiveColor(const std::string& activeSection) {
    //FIXME Mettere codizione in variabile costante booleana per aumentare la leggibilita' dell'operatore ternario
    return (!activeSection.empty() && (activeSection[0] == 'b' || activeSection[0] == 'B')) ? BLACK : WHITE;
}

Coords Board::parseEnPassant(const std::string& ep) {
    //FIXME Creare funzione helper per codizione
    if (ep.size() != 2 || ep == "-" || ep[0] < 'a' || ep[0] > 'h' || ep[1] < '1' || ep[1] > '8') return Coords{};
    return Coords(ep[0] - 'a', '8' - ep[1]);
}

uint8_t Board::safeParseInt(const std::string& section, int min, int max, int defaultValue) {
    if (section.empty()) return defaultValue;
    int v = 0;
    auto res = std::from_chars(section.data(), section.data() + section.size(), v);
    return (res.ec == std::errc{} && res.ptr == section.data() + section.size()) ? std::clamp(v, min, max) : defaultValue;
}

void Board::fromFenToBoard(const std::string& fen) {
    std::istringstream fenStream(fen);
    std::string board, active, castling, ep, half, full;
    if (!(fenStream >> board >> active >> castling >> ep >> half >> full)) return;

    std::array<uint32_t, 8> parsedBoard{};
    if (!parseBoardSection(board, parsedBoard)) return;

    chessboard = parsedBoard;
    activeColor = parseActiveColor(active);
    
    //FIXME Elimina costati magiche
    //FIXME Aggiungere this
    castle = 0;
    for (char c : castling) {
        if (c == 'K') castle |= 1;
        else if (c == 'Q') castle |= 2;
        else if (c == 'k') castle |= 4;
        else if (c == 'q') castle |= 8;
    }

    enPassant = parseEnPassant(ep);
    halfMoveClock = safeParseInt(half, 0, 255, 0);
    fullMoveClock = safeParseInt(full, 1, 255, 1);

    clearEvalCache();
    lastMoveChangeFlags = MOVE_CHANGE_NONE;
    updateOccupancyBB();
    rebuildRepetitionHistory();
}

std::string Board::boardToFenPieces() const {
    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int emptySq = 0;
        for (int file = 0; file < 8; ++file) {
            uint8_t p = (chessboard[rank] >> (file * 4)) & MASK_PIECE;
            if (p == EMPTY) { ++emptySq; continue; }
            if (emptySq) { fen += std::to_string(emptySq); emptySq = 0; }
            char sym = PIECE_TYPE_TO_CHAR[p & Board::MASK_PIECE_TYPE];
            fen += (p & MASK_COLOR) == BLACK ? static_cast<char>(std::tolower(static_cast<unsigned char>(sym))) : sym;
        }
        if (emptySq) fen += std::to_string(emptySq);
        if (rank > 0) fen += '/';
    }
    return fen;
}

//FIXME Rendere constexpr 
std::string Board::castlingToFen() const {
    std::string s;
    if (castle & 1) s += 'K';
    if (castle & 2) s += 'Q';
    if (castle & 4) s += 'k';
    if (castle & 8) s += 'q';
    return s.empty() ? "-" : s;
}

//FIXME Rendere constexpr 
std::string Board::enPassantToFen() const {
    if (!enPassant.isValid()) return "-";
    return std::string(1, 'a' + enPassant.file()) + static_cast<char>('8' - enPassant.rank());
}

//FIXME Rendere constexpr 
//FIXME Evitare monoriga per aumentare leggibilita'
std::string Board::fromBoardToFen() const {
    return boardToFenPieces() + " " + (activeColor == WHITE ? "w " : "b ") + castlingToFen() + " " + enPassantToFen() + " " + std::to_string(halfMoveClock) + " " + std::to_string(fullMoveClock);
}

}
