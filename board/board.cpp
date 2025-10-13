#include <sstream>
#include <cctype>
#include <algorithm>
#include "board.hpp"

namespace chess {

Board::Board() {
    static const std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    fromFenToBoard(STARTING_FEN);
}

/*
Board::Board(int empty) {
    std::string empty_fen = "8/8/8/8/8/8/8/8 w - - 0 1";
    fromFenToBoard(empty_fen);
}*/

Board::Board(std::string fen) {
    static const std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    fromFenToBoard(fen);
}

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

Piece& Board::operator[](std::size_t index) {
    return board[index];
}

const Piece& Board::operator[](std::size_t index) const {
    return board[index];
}

Piece& Board::at(const uint8_t position) { 
    return board.at(position);
}

const Piece& Board::at(const uint8_t position) const {
    return board.at(position);
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
            // TODO: forse è meglio creare un pezzo EMPTY statico e copiarlo
            // TODO: aggiungere le coordinate ai pezzi vuoti? 
            // Piece emptySquare {{static_cast<uint8_t>(file), static_cast<uint8_t>(rank)}, P_EMPTY, false};
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

std::string Board::getCurrentFen() {
    std::string fen;
    fen.reserve(71);
    int emptySquaresCounter = 0;

    for (long unsigned int i = 0; i < board.size(); ++i) {
        
        if (i > 0 && i % 8 == 0) { // nuova riga ogni 8 pezzi
            if (emptySquaresCounter > 0) {
                fen += std::to_string(emptySquaresCounter);
                emptySquaresCounter = 0;
            }
            fen.push_back('/');
        }

        const Piece& piece = board[i];

        if (piece.id == P_EMPTY) {
            ++emptySquaresCounter;
        } else {
            if (emptySquaresCounter > 0) {
                fen += std::to_string(emptySquaresCounter);
                emptySquaresCounter = 0;
            }

            char c;
            switch (piece.id) {
                case P_PAWN:   c = 'p'; break;
                case P_KNIGHT: c = 'n'; break;
                case P_BISHOP: c = 'b'; break;
                case P_ROOK:   c = 'r'; break;
                case P_QUEEN:  c = 'q'; break;
                case P_KING:   c = 'k'; break;
                default:       c = '?'; break;
            }

            fen.push_back(piece.isWhite ? std::toupper(c) : c);
        }
    }

    if (emptySquaresCounter > 0) { // edge case ultima riga => se ci sono caselle vuote non scritte
        fen.push_back('0' + emptySquaresCounter);
    }

    // turn
    fen.push_back(' ');
    fen.push_back(getIsWhiteTurn() ? 'w' : 'b');

    // castling
    fen.push_back(' ');
    std::array<bool, 4> castling = getCastling();
    std::string castlingStr;
    if (castling[0]) castlingStr += 'K';
    if (castling[1]) castlingStr += 'Q';
    if (castling[2]) castlingStr += 'k';
    if (castling[3]) castlingStr += 'q';
    fen.append(castlingStr.empty() ? "-" : castlingStr);

    // en passant
    fen.push_back(' ');
    Coords ep = getEnPassant();
    if (ep.file == 0 && ep.rank == 0) { // TODO: cambiare con le coordinate invalide
        fen.push_back('-');
    } else {
        fen.push_back(static_cast<char>('a' + ep.file));
        fen.push_back(static_cast<char>('1' + ep.rank));
    }

    // half moves and full moves
    fen.push_back(' ');
    fen.append(std::to_string(getHalfMoveClock()));
    fen.push_back(' ');
    fen.append(std::to_string(getFullMoveClock()));

    return fen;
}

uint8_t Board::fromCoordsToPosition(const Coords& coord) {
    return coord.file + coord.rank * 8;
}

Coords Board::fromPositionToCoords(const int position) {
    return { static_cast<uint8_t>(position % 8), static_cast<uint8_t>(position / 8) };
}

bool Board::movePiece(const Piece& current, const Coords& target) {
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
