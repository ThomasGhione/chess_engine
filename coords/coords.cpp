#include "coords.hpp"

namespace chess {

Coords::Coords()
    : file(INVALID_COORDS)
    , rank(INVALID_COORDS)
{}

Coords::Coords(uint8_t f, uint8_t r)
        : Coords()
{
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
    }

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
    if (this != &other) {
        file = other.file;
        rank = other.rank;
        return *this;
    }
}

void Coords::update(const Coords& other) {
    if (this->isValid(other.file)) {
        file = other.file;
    }
    if (this->isValid(other.rank)) {
        rank = other.rank;
    }
}


bool Coords::isValid(uint8_t x) const {
    return x >= 0 && x < 64;
}

bool Coords::isLetter(char c) const {
    return ((c >= 'a' && c <= 'h') || (c >= 'A' && c <= 'H'));
}

bool Coords::isNumber(char c) const {
    return (c >= '1' && c <= '8');
}

}