#ifndef BOARD_HPP
#define BOARD_HPP

/*
 * if(pezzo e' sto colore)
 *
 * f(bit colore) = 2x -1
 *
 * bit 0 -> nero -> -1
 * bit 1 -> bianco -> 1
 *
 * z(board)
 * sum 4 bit della board
 * ti prendi solo il bit colore (f(bit colore)) * (g(bit pezzo))
 *
 * Alla pos iniziale z(board) = 0
 *
 * Se pos fosse: 1 pedone bianco e i due re:
 * z(board) = +1
 *
 * Se regina e' il 5 pezzo e re il 5 e pedone il 1
 * Se pos fosse: 1 regina bianca e 1 pedone nero i due re:
 * z(board) = +4
 * */


//#include <string>
#include <array>
#include <cstdint>
//#include <sstream>
//#include <cctype>
#include <vector>
#include <tuple>
#include <algorithm>
#include <cstddef>
#include <unordered_map>
//#include "../piece/piece.hpp"

#include "../coords/coords.hpp"

// #include "../piece/pawn.hpp"
// #include "../piece/knight.hpp"
// #include "../piece/bishop.hpp"
// #include "../piece/rook.hpp"
// #include "../piece/knight.hpp"
// #include "../piece/queen.hpp"
// #include "../piece/king.hpp"

namespace chess {

using board = std::array<uint32_t, 8>;

class Board {

public:
    enum piece_id : uint8_t {
    // piece bits
    EMPTY  = 0x0, // 0000 
    PAWN   = 0x1, // 0001
    KNIGHT = 0x2, // 0010
    BISHOP = 0x3, // 0011
    ROOK   = 0x4, // 0100
    QUEEN  = 0x5, // 0101
    KING   = 0x6, // 0110
    // color bit
    BLACK  = 0x8, // 1000
    WHITE  = 0x0, // 0000

    // ENPASSANT = 0x7  // 0111
};


private:
    board chessboard; // 8 * 32 bit = 256 bit = 32 byte
    uint8_t castle = 0; // 4 bit for castling rights (KQkq)
    std::array<Coords, 2> enPassant = {Coords{}, Coords{}}; // WHITE and BLACK
    uint8_t halfMoveClock = 0; // Tracks the number of half-moves since the last pawn move or capture
    uint8_t fullMoveClock = 1; // Tracks the number of full moves in the game
    uint8_t activeColor = WHITE; // Tracks the active color (white or black)

    static constexpr uint8_t MASK_PIECE = 0x0F;      // 0000 1111
    static constexpr uint8_t MASK_COLOR = 0x08;      // 0000 1000
    static constexpr uint8_t MASK_PIECE_TYPE = 0x07; // 0000 0111

    // std::unordered_map<std::tuple<piece_id, Coords>, std::vector<chess::Coords>> legalMoves; //? maybe there's a better way?

public:

    Board() noexcept : chessboard{0} {}
    Board(const std::array<uint32_t, 8>& chessboard) noexcept
        : chessboard(chessboard)
        , castle(this->MASK_PIECE) // 0x0F = 0000 1111 => all castling rights available
        , enPassant({Coords(), Coords()}) 
        , halfMoveClock(0)
        , fullMoveClock(1)
        , activeColor(WHITE)
    {}
   
    //! GETTERS
    // assert(col <= 7)
    // assert(row <= 7)
    uint8_t get(Coords coords) const noexcept { return (chessboard.at(coords.rank) >> (coords.file * 4)) & this->MASK_PIECE; }
    constexpr uint8_t get(uint8_t row, uint8_t col) const noexcept { return (chessboard.at(row) >> (col * 4)) & this->MASK_PIECE; }

    //! SETTERS
    void set(Coords coords, uint8_t value) noexcept {
        const uint8_t shift = coords.file * 4;
        chessboard.at(coords.rank) = (chessboard.at(coords.rank) & ~(MASK_PIECE << shift)) | ((value & MASK_PIECE) << shift);
    }

    void set(uint8_t row, uint8_t col, uint8_t value) noexcept {
        const uint8_t shift = col * 4;
        chessboard.at(row) = (chessboard.at(row) & ~(MASK_PIECE << shift)) | ((value & MASK_PIECE) << shift);
    }

    void set_linear(uint8_t index, uint8_t value) noexcept { this->set(index % 8, index / 8, value); }
    
    constexpr uint8_t coordsToIndex(const Coords& coords) const noexcept {
        return coords.rank * 8 + coords.file;
    }

    //! Operator overloads
    uint8_t operator[](const Coords& coords) const noexcept { return this->get(coords); }
    uint8_t operator[](const Coords& coords) noexcept { return this->get(coords); }
    uint8_t operator[](uint8_t index) const noexcept { return this->get(index % 8, index / 8); } // assert index 0-63 r
    uint8_t operator[](uint8_t index) noexcept { return this->get(index % 8, index / 8); }
    bool operator==(const Board& other) const noexcept { return this->chessboard == other.chessboard; }
    bool operator!=(const Board& other) const noexcept { return this->chessboard != other.chessboard; }

    /*
    constexpr const std::array<uint32_t, 8>& chessboard() const noexcept {
        return chessboard;
    }
    
    void chessboard(const std::array<uint32_t, 8>& chessboard) noexcept {
        chessboard = chessboard;
    }*/
    
    /* 
    void clear() noexcept { 
        chessboard.fill(0); 
    }*/
    
    //! PER DEBUG
    static constexpr size_t size() noexcept { return sizeof(chessboard); } // 32 byte

    // Iterator support
    auto begin() noexcept { return chessboard.begin(); }
    auto end() noexcept { return chessboard.end(); }
    constexpr auto begin() const noexcept { return chessboard.begin(); }
    constexpr auto end() const noexcept { return chessboard.end(); }
    constexpr auto cbegin() const noexcept { return chessboard.cbegin(); }
    constexpr auto cend() const noexcept { return chessboard.cend(); }


    // Piece movement logic
    bool isSameColor(const Coords& pos1, const Coords& pos2) const noexcept {
        uint8_t p1 = this->get(pos1);
        uint8_t p2 = this->get(pos2);
        if (p1 == EMPTY || p2 == EMPTY) return false;
        return (p1 & BLACK) == (p2 & BLACK);
    }

    // TODO check whether this works or not?
    static uint8_t fromCoordsToPosition(const Coords& coords) {
        return coords.rank * 8 + coords.file;
    }


    bool move(Coords from, Coords to) noexcept {
        if (!canMoveTo(from, to))
            return false;
        uint8_t piece = this->get(from);
        this->set(to, piece);
        this->set(from, EMPTY);
        return true;
    }

    
    bool canMoveTo(const Coords& from, const Coords& to) const noexcept {
        std::vector<Coords> allLegalMoves = getAllLegalMoves(from);
        return std::find(allLegalMoves.cbegin(), allLegalMoves.cend(), to) != allLegalMoves.cend();
    }

    std::vector<Coords> getAllLegalMoves(const Coords& from) const noexcept {
        switch (this->get(from) & this->MASK_PIECE_TYPE) { // Mask to get piece type only
            // case PAWN: return Pawn::getPawnMoves(*this, from);
            // case KNIGHT: return Knight::getKnightMoves(*this, from);
            // case BISHOP: return Bishop::getBishopMoves(*this, from);
            // case ROOK: return Rook::getRookMoves(*this, from);  // TODO implement castling
            // case QUEEN: return Queen::getQueenMoves(*this, from);
            // case KING: return King::getKingMoves(*this, from); // TODO implement castling, check, checkmate, stalemate
        }
        return {};
    }


    /*
public:
    Board(); // Default constructor => starting position
    Board(const std::string& fen);
    Board(const Board& b) = default;

    void createEmpty();

    bool isCurrentPositionMate();
    std::string getCurrentPositionFen();
    
    std::array<Piece, 64> board;

    std::string getCurrentFen();
    
    bool getIsWhiteTurn() const {return this->isWhiteTurn;}
    std::array<bool, 4> getCastling() const {return this->castle;}
    chess::Coords getEnPassant() const {return this->enPassant;}
    uint8_t getHalfMoveClock() const {return this->halfMoveClock;}
    uint8_t getFullMoveClock() const {return this->fullMoveClock;}

    static uint8_t fromCoordsToPosition(const Coords& coords);
    static Coords fromPositionToCoords(const uint8_t position);

    bool movePiece(const Piece& current, const Coords& target);

    Board& operator=(const chess::Board& other);
    Piece& operator[](std::size_t index);
    const Piece& operator[](std::size_t index) const;

    Piece& at(const uint8_t position);
    const Piece& at(const uint8_t position) const;
    
private:
    static const std::string STARTING_FEN;
    
    bool isWhiteTurn;
    std::array<bool, 4> castle;
    Coords enPassant;
    uint8_t halfMoveClock;
    uint8_t fullMoveClock;
    
    void fromFenToBoard(std::string fen);
*/

};

} // namespace chess

#endif
