#ifndef BOARD_HPP
#define BOARD_HPP

#include <string>
#include <array>
#include <cstdint>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <cstddef>
#include "../piece/piece.hpp"


namespace chess {

class Board {

private:
    std::array<uint32_t, 8> data; // 8 × 32 bit = 256 bit = 32 byte

public:
    // Costruttori
    constexpr Board() noexcept : data{0} {}
    constexpr Board(const std::array<uint32_t, 8>& initial_data) noexcept
        : data(initial_data) {}
    
    // Accesso diretto - massima efficienza
    constexpr uint8_t get(int row, int col) const noexcept {
        return (data[row] >> (col * 4)) & 0x0F;
    }
    
    constexpr void set(int row, int col, uint8_t value) noexcept {
        constexpr uint32_t MASK = 0x0F;
        const int shift = col * 4;
        data[row] = (data[row] & ~(MASK << shift)) | ((value & MASK) << shift);
    }
    
    // Accesso per indice lineare (0-63)
    constexpr uint8_t operator[](int index) const noexcept {
        return get(index / 8, index % 8);
    }
    
    constexpr void set_linear(int index, uint8_t value) noexcept {
        set(index / 8, index % 8, value);
    }
    
    // Operazioni bulk sulle righe - MOLTO efficienti per algoritmi scacchistici
    constexpr uint32_t get_row(int row) const noexcept { 
        return data[row]; 
    }
    
    constexpr void set_row(int row, uint32_t value) noexcept { 
        data[row] = value; 
    }
    
    // Accesso diretto ai dati (per operazioni avanzate)
    constexpr const std::array<uint32_t, 8>& get_data() const noexcept {
        return data;
    }
    
    constexpr void set_data(const std::array<uint32_t, 8>& new_data) noexcept {
        data = new_data;
    }
    
    // Operazioni di confronto e utility
    constexpr bool operator==(const Board& other) const noexcept {
        return data == other.data;
    }
    constexpr bool operator!=(const Board& other) const noexcept {
        return data != other.data;
    }
    
    constexpr void clear() noexcept { 
        data.fill(0); 
    }
    
    // Dimensione garantita
    static constexpr size_t size() noexcept { 
        return sizeof(data); // 32 byte
    }
    
    // Iteratori per algoritmi generici
    constexpr auto begin() noexcept { 
        return data.begin(); 
    }
    
    constexpr auto end() noexcept { 
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
    int getHalfMoveClock() const {return this->halfMoveClock;}
    int getFullMoveClock() const {return this->fullMoveClock;}

    static uint8_t fromCoordsToPosition(const Coords& coords);
    static Coords fromPositionToCoords(const int position);

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
    int halfMoveClock;
    int fullMoveClock;
    
    void fromFenToBoard(std::string fen);
*/



};



} // namespace chess

#endif
