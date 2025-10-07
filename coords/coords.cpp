#include "coords.hpp"

namespace chess {

Coords::Coords()
    : file(EMPTY)
    , rank(EMPTY)
{}

Coords::Coords(uint8_t f, uint8_t r)
        : Coords()
{
    if (isValid(f)) {
        file = f;
    }
    if (isValid(r)) {
        rank = r;
    }
}

Coords::Coords(std::string input)
    : Coords()
{
    if (input.length() != 2) {
        // TODO: gestire l'errore
    }

    if (isValid(input[0] - 'a')) {
        file = input[0] - 'a';
    }
    if (isValid(input[1] - '1')) {
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
    if (isValid(other.file)) {
        file = other.file;
    }
    if (isValid(other.rank)) {
        rank = other.rank;
    }
}


bool Coords::isValid(uint8_t x) const {
    return x >= 0 && x < 64;
}

}