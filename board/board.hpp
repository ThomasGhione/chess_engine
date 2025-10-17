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
//#include <algorithm>
#include <cstddef>
//#include "../piece/piece.hpp"

namespace chess {

class Board {

private:
  std::array<uint32_t, 8> data; // 8 * 32 bit = 256 bit = 32 byte
  constexpr uint8_t BIT_MASK_4_BITS = 0x0F;


public:
    constexpr Board() noexcept : data{0} {}
    constexpr Board(const std::array<uint32_t, 8>& initial_data) noexcept
        : data(initial_data) {}
   
    constexpr uint8_t get(uint8_t row, uint8_t col) const noexcept {
      // assert(col <= 7)
      // assert(row <= 7)
      return (data.at(row) >> (col * 4)) & this->BIT_MASK_4_BITS;
    }
    
    void set(uint8_t row, uint8_t col, uint8_t value) noexcept {
      // row = row & MASK(00000000...111)
        const uint8_t shift = col * 4;
        data.at(row) = (data.at(row) & ~(BIT_MASK_4_BITS << shift)) | ((value & BIT_MASK_4_BITS) << shift);
    }
    
    // assert index 0-63
    constexpr uint8_t operator[](uint8_t index) const noexcept {
        return this->get(index / 8, index % 8);
    }
    
    void set_linear(uint8_t index, uint8_t value) noexcept {
        this->set(index / 8, index % 8, value);
    }
    
    // assert row <= 7
    constexpr uint32_t get_row(uint8_t row) const noexcept {
        return data.at(row);
    }
    
    // assert row <= 7
    void set_row(uint8_t row, uint32_t value) noexcept { 
        data.at(row) = value; 
    }
    
    /*
    constexpr const std::array<uint32_t, 8>& get_data() const noexcept {
        return data;
    }
    
    void set_data(const std::array<uint32_t, 8>& new_data) noexcept {
        data = new_data;
    }*/
    
    constexpr bool operator==(const Board& other) const noexcept {
        return this->data == other.data;
    }
    constexpr bool operator!=(const Board& other) const noexcept {
        return this->data != other.data;
    }
    /* 
    void clear() noexcept { 
        data.fill(0); 
    }*/
    
    static constexpr size_t size() noexcept { //! PER DEBUG
        return sizeof(data); // 32 byte
    }
    
    auto begin() noexcept { 
        return data.begin(); 
    }
    
    auto end() noexcept { 
        return data.end(); 
    }
    
    constexpr auto begin() const noexcept { 
        return data.begin(); 
    }
    
    constexpr auto end() const noexcept { 
        return data.end(); 
    }
    
    constexpr auto cbegin() const noexcept { 
        return data.cbegin(); 
    }
    
    constexpr auto cend() const noexcept { 
        return data.cend(); 
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
