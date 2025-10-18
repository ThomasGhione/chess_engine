#include "coords.hpp"

namespace chess {

Coords::Coords()
    : file(INVALID_COORDS)
    , rank(INVALID_COORDS)
{}

Coords::Coords(uint8_t f, uint8_t r)
        : Coords()
{
  //TODO chiamata a funzione che controlla altre cose.
    if (this->isValid(f)) {
        file = f;
    }
    if (this->isValid(r)) {
        rank = r;
    }
}

Coords::Coords(const std::string& input)
    : Coords()
{
    if (input.length() != 2) {
        // TODO: gestire l'errore
      return;
    }

    if (this->isLetter(std::tolower(input[0]))) {
        file = std::tolower(input[0]) - 'a';
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
    //! Messo solo per non avere errore, da sistemare!
    return *this;
}

bool Coords::update(const Coords& other) {
    if (this->file == INVALID_COORDS || this->rank == INVALID_COORDS) {
        return false;
    }
    this->file = other.file;
    this->rank = other.rank;
    return true;
}

bool Coords::update(const uint8_t f, const uint8_t r) {
    if (this->file == INVALID_COORDS || this->rank == INVALID_COORDS) {
        return false;
    }
    this->file = f;
    this->rank = r;
    return true;
}


bool Coords::isValid(uint8_t x) {
    return (x < 64);
}

bool Coords::isLetter(char c) {
    return ((c >= 'a' && c <= 'h') || (c >= 'A' && c <= 'H'));
}

bool Coords::isNumber(char c) {
    return (c >= '1' && c <= '8');
}

bool isInBounds(const Coords& coords) {
    return ((coords.file < 8 && coords.rank < 8) && (coords.file >= 0 && coords.rank >= 0));
}

}
