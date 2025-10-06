#include <sstream>
#include <cctype>
#include "board.hpp"

namespace chess {

Board::Board(std::string fen) {
    fromFenToBoard(fen);
}

void Board::fromFenToBoard(std::string fen) {
    if (fen.empty()) {
        fen = STARTING_FEN;
    }

    std::istringstream fenStream(fen);
    std::string token;
    
    // 1. Parsing posizione pezzi
    fenStream >> token;
    
    int rank = 7;
    int file = 0;
    
    for (char c : token) {
        if (c == '/') { // '/' stands for new line
            rank--;
            file = 0;
        }
        else if (std::isdigit(c)) { // empty squares
            file += (c - '0');
        }
        else { // piece
            Piece piece {{static_cast<uint8_t>(file), static_cast<uint8_t>(rank)}, P_EMPTY, false};
            
            char aux = std::tolower(c);
            switch (aux) {
                case 'p': piece.id = P_PAWN; break;
                case 'r': piece.id = P_ROOK; break; 
                case 'n': piece.id = P_KNIGHT; break;
                case 'b': piece.id = P_BISHOP; break;
                case 'q': piece.id = P_QUEEN; break;
                case 'k': piece.id = P_KING; break;
                default: break; // P_NONE già impostato
            }
            
            piece.isWhite = std::isupper(c);
            board[rank * 8 + file] = piece;
            file++;
        }
    }
    
    // 2. Turno
    fenStream >> token;
    isWhiteTurn = (token == "w");
    
    // 3. Arrocchi
    fenStream >> token;
    castle[0] = castle[1] = castle[2] = castle[3] = false;
    
    if (token != "-") {
        for (char c : token) {
            switch (c) {
                case 'K': castle[0] = true; break;
                case 'Q': castle[1] = true; break;
                case 'k': castle[2] = true; break;
                case 'q': castle[3] = true; break;
            }
        }
    }
    
    // 4. En passant
    fenStream >> token;
    if (token != "-") {
        enPassant = {static_cast<uint8_t>(token[0] - 'a'), 
                     static_cast<uint8_t>(token[1] - '1')};
    }
    else {
        enPassant = {0, 0}; // TODO: forse meglio usare coordinate invalide tipo {-1,-1}?
    }
    
    // 5. Half-move clock
    fenStream >> token;
    halfMoveClock = std::stoi(token);
    
    // 6. Full-move number
    fenStream >> token;
    fullMoveClock = std::stoi(token);
}

inline uint8_t Board::fromCoordsToPosition(const coords& coord) {
    return coord.file + coord.rank * 8;
}

inline coords Board::fromPositionToCoords(const int& position) {
    return { static_cast<uint8_t>(position % 8), static_cast<uint8_t>(position / 8) };
}

void Board::updatePiecePosition(const Piece& current, const coords& target) {
    board[fromCoordsToPosition(current.coords)] = EMPTY; // TODO: come eliminare il pezzo dalla casella?
    board[fromCoordsToPosition(target)] = current;  // assegno il vecchio pezzo nella nuova posizione
    board[fromCoordsToPosition(target)].coords = target; // aggiorno le coordinate del pezzo
}


} // namespace chess