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
        this->file = f;
    }
    if (this->isValid(r)) {
        this->rank = r;
    }
}

Coords::Coords(const std::string& input)
    : Coords()
{
    if (input.length() != 2) {
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
    return (this->file == other.file) && (this->rank == other.rank);
}

bool Coords::operator!=(const Coords &other) const {
    return !(*this == other);
}

Coords& Coords::operator=(const Coords &other) {
    if (this != &other) {
        this->file = other.file;
        this->rank = other.rank;
    }
    return *this;
}

bool Coords::update(const Coords& other) {
    return this->update(other.file, other.rank);
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
    return (c >= '1') && (c <= '8');
}

bool Coords::isInBounds(const Coords& coords) {
    return (coords.file < 8) && (coords.rank < 8);
}

std::string Coords::toString() const {
    std::string result(2, ' ');
    if (this->file < 8 && this->rank < 8) {
        result[0] = static_cast<char>('a' + this->file);
        result[1] = static_cast<char>('1' + this->rank);
    } else {
        result = "??";
    }
    return result;

}

}