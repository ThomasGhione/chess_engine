#include "coords.hpp"

namespace chess {

Coords::Coords()
    : file(INVALID_COORDS)
    , rank(INVALID_COORDS)
{}

Coords::Coords(uint8_t f, uint8_t r)
        : Coords()
{
  //TOBE FIXED chiamata a funzione che controlla altre cose.
    if (this->isValid(f)) {
        file = f;
    }
    if (this->isValid(r)) {
        rank = r;
    }
}

Coords::Coords(std::string input)
    : Coords()
{
    if (input.length() != 2) {
        // TODO: gestire l'errore
      return;
    }
    
    // TOBE FIXED se lettera maiuscola invece che minuscola.
    if (this->isLetter(input[0])) {
        file = input[0] - 'a';
    }
    if (this->isNumber(input[1])) {
        rank = input[1] - '1';
    }
}

bool Coords::operator==(const Coords &other) const {
    return (file == other.file && rank == other.rank);
}

bool Coords::operator!=(const Coords &other) const {
    return !(*this == other);
}

Coords& Coords::operator=(const Coords &other) {
  // TOBE FIXED: Se sono uguali non ritorna nulla.
    if (this != &other) {
        file = other.file;
        rank = other.rank;
        return *this;
    }
    // Messo solo per non avere errore, da sistemare!
    return *this;
}

void Coords::update(const Coords& other) {
  // TOBE FIXED: Avendo other istanza di Cooords
  // il controllo isValid passa sempre.
  // Andrebbe controllato solo che sia diverso da 9 (coordina illegale)

    if (this->isValid(other.file)) {
        file = other.file;
    }
    if (this->isValid(other.rank)) {
        rank = other.rank;
    }
}

void Coords::update(const uint8_t f, const uint8_t r) {
    if (this->isValid(f)) {
        file = f;
    }
    if (this->isValid(r)) {
        rank = r;
    }
}


bool Coords::isValid(uint8_t x) const {
    return (x < 64);
}

bool Coords::isLetter(char c) const {
    return ((c >= 'a' && c <= 'h') || (c >= 'A' && c <= 'H'));
}

bool Coords::isNumber(char c) const {
    return (c >= '1' && c <= '8');
}

}
