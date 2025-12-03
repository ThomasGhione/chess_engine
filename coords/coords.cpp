#include "coords.hpp"

namespace chess {

Coords::Coords()
    : file(INVALID_COORDS)
    , rank(INVALID_COORDS)
{}

Coords::Coords(uint8_t index)
    : Coords()
{
    if (index < 64) {
        this->file = index % 8;
        this->rank = index / 8;
    }
}

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
    if (f == INVALID_COORDS || r == INVALID_COORDS) {
        return false;
    }

    this->file = f;
    this->rank = r;
    return true;
}


bool Coords::isValid(uint8_t x) noexcept {
    return (x < 8);
}

bool Coords::isLetter(char c) noexcept {
    return ((c >= 'a' && c <= 'h') || (c >= 'A' && c <= 'H'));
}

bool Coords::isNumber(char c) noexcept {
    return (c >= '1') && (c <= '8');
}

bool Coords::isInBounds(const Coords& coords) noexcept {
    return (isValid(coords.file) && isValid(coords.rank));
}

std::string Coords::toString() const {
    std::string result(2, ' ');
    if (isInBounds(*this)) {
        result[0] = static_cast<char>('a' + this->file);
        result[1] = static_cast<char>('1' + this->rank);
    } else {
        result = "??";
    }
    return result;

}

uint8_t Coords::toIndex() const noexcept {
    return static_cast<uint8_t>(this->rank * 8 + this->file);
}


std::string Coords::toAlgebric(const Coords& c) noexcept {
    char fileChar = static_cast<char>('a' + c.file);
    char rankChar = static_cast<char>('1' + c.rank);
    std::string s;
    s.push_back(fileChar);
    s.push_back(rankChar);
    return s;
}

}