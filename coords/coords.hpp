#ifndef COORDS
#define COORDS

#include <string>
#include <cstdint>
#include <cctype>

namespace chess {
class Coords {
public:
    constexpr static uint8_t INVALID_COORDS = 255;

    // Storage efficiente: solo 1 byte invece di 2
    // Convenzione Board: a8=0, b8=1, ..., h8=7, a7=8, ..., h1=63

    uint8_t index = INVALID_COORDS;

    // ============== COSTRUTTORI ==============

    // Costruttore di default: coordinate invalide
    constexpr Coords() noexcept = default;

    // Costruttore da index (0-63)
    // Usa validazione inline per efficienza
    constexpr explicit Coords(uint8_t idx) noexcept
        : index(idx < 64 ? idx : INVALID_COORDS) {}

    // PERFORMANCE: Unchecked constructor for known-valid indices (engine hot path)
    // WARNING: caller MUST guarantee idx < 64
    struct Unchecked {};
    constexpr Coords(uint8_t idx, Unchecked) noexcept : index(idx) {}

    // Costruttore da file e rank (0-7 ciascuno)
    // Formula: index = rank * 8 + file
    constexpr Coords(uint8_t f, uint8_t r) noexcept
        : index((f < 8 && r < 8) ? static_cast<uint8_t>(r * 8 + f) : INVALID_COORDS) {}

    // Costruttore da stringa notazione algebrica (es. "e4", "a8")
    // NON constexpr perché usa std::tolower che non è constexpr
    explicit Coords(const std::string& input) noexcept : index(INVALID_COORDS) {
        if (input.length() != 2) return;

        char fileChar = std::tolower(input[0]);
        char rankChar = input[1];

        if (!isLetter(fileChar) || !isNumber(rankChar)) return;

        // Converti notazione algebrica a index interno
        // file: 'a'=0, 'b'=1, ..., 'h'=7
        uint8_t f = static_cast<uint8_t>(fileChar - 'a');

        // rank: '8'=0, '7'=1, ..., '1'=7 (convenzione a8=0, h1=63)
        // Formula: rank_interno = '8' - rankChar
        uint8_t r = static_cast<uint8_t>('8' - rankChar);

        this->index = r * 8 + f;
    }

    // Copy constructor (default va bene)
    constexpr Coords(const Coords& other) noexcept = default;

    // ============== METODI DI ACCESSO PUBBLICI ==============

    // Restituisce la colonna (0-7: a-h)
    // Usa bit-mask per performance ottimale (equivale a index % 8)
    constexpr uint8_t file() const noexcept {
        return this->index & 7;
    }

    // Restituisce la riga (0-7: dove 0=riga8, 7=riga1 per convenzione Board)
    // Usa bit-shift per performance ottimale (equivale a index / 8)
    constexpr uint8_t rank() const noexcept {
        return this->index >> 3;
    }

    // Verifica se le coordinate sono valide
    constexpr bool isValid() const noexcept {
        return this->index < 64;
    }

    // ============== OPERATOR OVERLOADS ==============

    constexpr bool operator==(const Coords& other) const noexcept {
        return this->index == other.index;
    }

    constexpr bool operator!=(const Coords& other) const noexcept {
        return this->index != other.index;
    }

    constexpr Coords& operator=(const Coords& other) noexcept = default;

    // ============== SETTERS / UPDATERS ==============

    // Aggiorna da un altro oggetto Coords
    constexpr bool update(const Coords& other) noexcept {
        if (!other.isValid()) return false;
        this->index = other.index;
        return true;
    }

    // Aggiorna da file e rank
    constexpr bool update(uint8_t f, uint8_t r) noexcept {
        if (f >= 8 || r >= 8) return false;
        this->index = r * 8 + f;
        return true;
    }

    // Aggiorna da index
    constexpr bool update(uint8_t idx) noexcept {
        if (idx >= 64) return false;
        this->index = idx;
        return true;
    }

    // ============== UTILITY STATIC METHODS ==============

    // Verifica se un valore è nel range 0-7
    static constexpr bool isValid(uint8_t x) noexcept {
        return x < 8;
    }

    // Verifica se un carattere è una lettera di colonna valida
    static constexpr bool isLetter(char c) noexcept {
        return (c >= 'a' && c <= 'h') || (c >= 'A' && c <= 'H');
    }

    // Verifica se un carattere è un numero di riga valido
    static constexpr bool isNumber(char c) noexcept {
        return c >= '1' && c <= '8';
    }

    // Verifica se un oggetto Coords ha coordinate valide
    static constexpr bool isInBounds(const Coords& coords) noexcept {
        return coords.isValid();
    }

    // ============== CONVERSION METHODS ==============

    // Converti a stringa notazione algebrica (es. "e4")
    std::string toString() const noexcept {
        if (!this->isValid()) {
            return "??";
        }

        std::string result(2, ' ');
        uint8_t f = this->file();
        uint8_t r = this->rank();

        // file: 0-7 → 'a'-'h'
        result[0] = static_cast<char>('a' + f);

        // rank: 0-7 → '8'-'1' (convenzione a8=0, h1=63)
        result[1] = static_cast<char>('8' - r);

        return result;
    }

    // Converti a index (deprecato, usa index() invece)
    // Mantenuto per compatibilità
    constexpr uint8_t toIndex() const noexcept {
        return this->index;
    }

    // Metodo statico per conversione a notazione algebrica
    static std::string toAlgebric(const Coords& c) noexcept {
        return c.toString();
    }

}; // class Coords
} // namespace chess
#endif
