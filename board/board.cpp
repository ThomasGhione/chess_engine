#include <sstream>
#include <cctype>
#include <algorithm>
#include "board.hpp"

namespace chess {

Board::Board() {
    fromFenToBoard(STARTING_FEN);
}

Board::Board(std::string fen) {
    fromFenToBoard(fen);
}

// getters
bool Board::getIsWhiteTurn() const { return isWhiteTurn; }
std::array<bool, 4> Board::getCastling() const { return castle; }
Coords Board::getEnPassant() const { return enPassant; }
int Board::getHalfMoveClock() const { return halfMoveClock; }
int Board::getFullMoveClock() const { return fullMoveClock; }


Board& Board::operator=(const Board& other) {
    if (this != &other) {
        board = other.board;
        isWhiteTurn = other.isWhiteTurn;
        castle = other.castle;
        enPassant = other.enPassant;
        halfMoveClock = other.halfMoveClock;
        fullMoveClock = other.fullMoveClock;
    }
    return *this;
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

std::string Board::fromBoardToFen() {
    std::string fen = "";

    int numOfEmptySquares = 0;

    // Board
    for (int i = 0; i < board.size(); i++) {
        if (i % 8 == 0 && i > 0) {
            numOfEmptySquares = 0;
            fen.push_back('/');
        }

        switch (board[i].id)
        {
            case P_EMPTY:
                ++numOfEmptySquares;

                if (numOfEmptySquares == 1)
                    fen.push_back('0' + numOfEmptySquares);
                else 
                    fen[fen.size()-1] = '0' + numOfEmptySquares;

                break;

            case P_PAWN:
                fen.push_back(board[i].isWhite ? 'P' : 'p');
                numOfEmptySquares = 0;

                break;

            case P_KNIGHT:
                fen.push_back(board[i].isWhite ? 'N' : 'n');
                numOfEmptySquares = 0;

                break;

            case P_BISHOP:
                fen.push_back(board[i].isWhite ? 'B' : 'b');
                numOfEmptySquares = 0;

                break;        

            case P_ROOK:
                fen.push_back(board[i].isWhite ? 'R' : 'r');
                numOfEmptySquares = 0;

                break;

            case P_QUEEN:
                fen.push_back(board[i].isWhite ? 'Q' : 'q');
                numOfEmptySquares = 0;
            
                break;

            case P_KING:
                fen.push_back(board[i].isWhite ? 'K' : 'k');
                numOfEmptySquares = 0;

                break;

            default:
                // There should be an exception
                break;
        }
    }

    fen.push_back(' ');

    // Turn
    fen.push_back(getIsWhiteTurn() ? 'w' : 'b');
    fen.push_back(' ');

    // Castling
    auto castling = getCastling();
    if (castling[0]) fen.push_back('K');
    if (castling[1]) fen.push_back('Q');
    if (castling[2]) fen.push_back('k');
    if (castling[3]) fen.push_back('q');
    if (!castling[0] && !castling[1] && !castling[2] && !castling[3]) 
        fen.push_back('-');
    fen.push_back(' ');

    // En passant
    auto ep = getEnPassant();
    if (ep.file == 0 && ep.rank == 0) {
        fen.push_back('-');
    } else {
        fen.push_back('a' + ep.file);
        fen.push_back('1' + ep.rank);
    }
    fen.push_back(' ');

    // Half-move clock
    fen += std::to_string(getHalfMoveClock());
    fen.push_back(' ');

    // Full-move number
    fen += std::to_string(getFullMoveClock());

    return fen;
}

inline uint8_t Board::fromCoordsToPosition(const Coords& coord) const {
    return coord.file + coord.rank * 8;
}

inline Coords Board::fromPositionToCoords(const int position) const {
    return { static_cast<uint8_t>(position % 8), static_cast<uint8_t>(position / 8) };
}

bool Board::updatePiecePosition(const Piece& current, const Coords& target) {
    // TODO: interrogare il pezzo
    // TODO: se è una mossa lecita, allora:

    Coords currentCoords = current.coords;

    uint8_t currentPos = fromCoordsToPosition(currentCoords);
    uint8_t targetPos = fromCoordsToPosition(target);
    
    // if (!board.at(currentPos).move(target)) {
    //     return false;
    // }

    std::swap(board.at(currentPos),board.at(targetPos));   // aggiorniamo il pezzo
    board[currentPos] = { currentCoords, P_EMPTY, false }; // in caso un pezzo mangia un altro
    board[targetPos].coords = target;                      // aggiorno le coordinate del pezzo

    return true;
}


}
/*
    e2->e4
    1) BOARD - copiare il pezzo in e4
    2) BOARD - eliminare il pezzo in e2
    3) PIECE - aggiornare le coordinate interne a piece
*/