# Magic Bitboards - Piano di Implementazione

**Data**: 2026-01-04
**Obiettivo**: Sostituire ray-based bitboards con magic bitboards per Rook e Bishop

---

## 1. Situazione Attuale

### Implementazione Corrente (piece/piece.hpp)

**Sliding pieces (Rook, Bishop, Queen)** usano attualmente un approccio **ray-based**:

```cpp
// Linee 101-103
inline constexpr U64 getRookAttacks(int8_t sq, U64 occ) noexcept {
    return ray(sq, N, occ) | ray(sq, S, occ) | ray(sq, E, occ) | ray(sq, W, occ);
}

// Linee 97-99
inline constexpr U64 getBishopAttacks(int8_t sq, U64 occ) noexcept {
    return ray(sq, NE, occ) | ray(sq, NW, occ) | ray(sq, SE, occ) | ray(sq, SW, occ);
}
```

La funzione `ray()` (linee 69-92):
- Usa `RAY_MASK[square][direction]` pre-calcolata
- Trova blockers: `U64 blockers = mask & occupancy`
- Switch su direzione (branch!)
- Usa `__builtin_ctzll` o `__builtin_clzll` per trovare primo/ultimo blocker
- Maschera il risultato

**Costo per chiamata**:
- Rook: 4 chiamate a `ray()` = ~40-60 cicli CPU
- Bishop: 4 chiamate a `ray()` = ~40-60 cicli CPU
- Contiene branches (switch statement)

### Lookup Tables Pre-Calcolate

Già implementate per pezzi non-sliding:
- `KNIGHT_ATTACKS[64]` - linee 273-281
- `KING_ATTACKS[64]` - linee 283-291
- `PAWN_ATTACKS[2][64]` - linee 248-257

---

## 2. Cosa sono le Magic Bitboards

### Idea Base

Invece di calcolare attacks on-the-fly, usa **perfect hashing** per lookup O(1):

```cpp
attacks = ATTACK_TABLE[square][magicIndex(occupancy)]
```

dove:
```cpp
magicIndex(occupancy, square) = ((occupancy & relevantSquares) * hashingConstant) >> indexShift
```

### Componenti

1. **Relevant Occupancy Mask**: Solo le square che influenzano davvero gli attacks
   - Per Rook in A1: square interne del rank/file (esclusi bordi)
   - Riduce da 14 bit a ~12 bit

2. **Magic Number**: Numero speciale uint64_t che crea hash perfetto
   - Trovato tramite trial-and-error
   - Garantisce nessuna collisione per tutte le configurazioni

3. **Attack Table**: Pre-calcolata per tutte le combinazioni occupancy/square
   - Rook: ~800 KB totale
   - Bishop: ~40 KB totale

### Vantaggi vs Ray-Based

| Aspetto | Ray-Based (attuale) | Magic Bitboards |
|---------|---------------------|-----------------|
| **Operazioni** | 4x ray(), switch, builtin | mask, mul, shift, load |
| **Cicli CPU** | ~40-60 | ~3-5 |
| **Branches** | Sì (switch) | No |
| **Cache** | Buono (mask table) | Ottimo (attack table) |
| **Memoria** | ~8 KB | ~840 KB |

**Speedup atteso**: 8-12x per singola chiamata
**Speedup engine**: 5-15% totale (dipende da tempo in move generation)

---

## 3. Architettura Proposta

### 3.1 Struttura Dati

```cpp
namespace pieces {

struct MagicData {
    uint64_t  relevantSquares;      // Square che influenzano gli attacks
    uint64_t  hashingConstant;      // Costante per perfect hashing
    uint32_t  indexShift;           // Shift per estrarre indice hash
    uint64_t* attackTable;          // Puntatore alla tabella pre-calcolata

    // Calcola l'indice hash per una data occupancy
    constexpr uint32_t computeIndex(uint64_t occupancy) const noexcept {
        return static_cast<uint32_t>(
            ((occupancy & relevantSquares) * hashingConstant) >> indexShift
        );
    }

    // Lookup attacks (encapsulamento completo)
    constexpr uint64_t getAttacks(uint64_t occupancy) const noexcept {
        return attackTable[computeIndex(occupancy)];
    }

    // Inizializza la struct con mask, magic number e table pointer
    void initialize(uint64_t relevantSquares, uint64_t hashingConstant, uint64_t* attackTable) noexcept {
        this->relevantSquares = relevantSquares;
        this->hashingConstant = hashingConstant;
        this->indexShift = 64 - __builtin_popcountll(relevantSquares);
        this->attackTable = attackTable;
    }
};

} // namespace pieces
```

### 3.2 Tabelle Globali

```cpp
#include <array>
#include <cstdint>

namespace pieces {

// Dimensioni lookup tables
constexpr size_t ROOK_LOOKUP_SIZE = 102400;    // ~800 KB
constexpr size_t BISHOP_LOOKUP_SIZE = 5248;    // ~40 KB

// Magic info per ogni square (inline per header-only)
inline std::array<pieces::MagicData, 64> ROOK_MAGIC_INFO;
inline std::array<pieces::MagicData, 64> BISHOP_MAGIC_INFO;

// Attack lookup tables (inline per header-only)
inline std::array<uint64_t, ROOK_LOOKUP_SIZE> ROOK_ATTACK_LOOKUP;
inline std::array<uint64_t, BISHOP_LOOKUP_SIZE> BISHOP_ATTACK_LOOKUP;

} // namespace pieces
```

### 3.3 Funzioni di Lookup

```cpp
namespace pieces {

inline uint64_t getRookAttacks(int square, uint64_t occupancy) noexcept {
    return pieces::ROOK_MAGIC_INFO[square].getAttacks(occupancy);
}

inline uint64_t getBishopAttacks(int square, uint64_t occupancy) noexcept {
    return pieces::BISHOP_MAGIC_INFO[square].getAttacks(occupancy);
}

inline uint64_t getQueenAttacks(int square, uint64_t occupancy) noexcept {
    return pieces::getRookAttacks(square, occupancy) | pieces::getBishopAttacks(square, occupancy);
}

} // namespace pieces
```

---

## 4. Piano di Implementazione

### Step 1: Generare Relevant Occupancy Masks

**Rook**: Rank + File, ESCLUSI i bordi (bordi non influenzano attacks)

```cpp
namespace pieces {

constexpr uint64_t rookRelevantMask(chess::Coords square) noexcept {
    uint64_t mask = 0ULL;
    int8_t file = square.file();
    int8_t rank = square.rank();

    // Riga (escludi bordi sinistro/destro)
    for (int8_t f = 1; f < 7; ++f) {
        if (f != file) {
            mask |= (1ULL << (rank * 8 + f));
        }
    }

    // Colonna (escludi bordi alto/basso)
    for (int8_t r = 1; r < 7; ++r) {
        if (r != rank) {
            mask |= (1ULL << (r * 8 + file));
        }
    }

    return mask;
}

} // namespace pieces
```

**Bishop**: Diagonali, ESCLUSI i bordi

```cpp
namespace pieces {

constexpr uint64_t bishopRelevantMask(chess::Coords square) noexcept {
    uint64_t mask = 0ULL;
    int8_t file = square.file();
    int8_t rank = square.rank();

    // Diagonale NE (escluso bordo)
    for (int8_t f = file + 1, r = rank - 1; f < 7 && r > 0; ++f, --r) {
        mask |= (1ULL << (r * 8 + f));
    }

    // Diagonale NW (escluso bordo)
    for (int8_t f = file - 1, r = rank - 1; f > 0 && r > 0; --f, --r) {
        mask |= (1ULL << (r * 8 + f));
    }

    // Diagonale SE (escluso bordo)
    for (int8_t f = file + 1, r = rank + 1; f < 7 && r < 7; ++f, ++r) {
        mask |= (1ULL << (r * 8 + f));
    }

    // Diagonale SW (escluso bordo)
    for (int8_t f = file - 1, r = rank + 1; f > 0 && r < 7; --f, ++r) {
        mask |= (1ULL << (r * 8 + f));
    }

    return mask;
}

} // namespace pieces
```

### Step 2: Generare Occupancy Patterns

Dato un mask con N bit settati, genera tutte le 2^N combinazioni:

```cpp
std::vector<uint64_t> generateOccupancyPatterns(uint64_t mask) {
    std::vector<int> bits;
    for (int sq = 0; sq < 64; sq++) {
        if (mask & (1ULL << sq)) bits.push_back(sq);
    }

    int n = bits.size();
    int patterns = 1 << n;
    std::vector<uint64_t> result(patterns);

    for (int i = 0; i < patterns; i++) {
        uint64_t occ = 0;
        for (int j = 0; j < n; j++) {
            if (i & (1 << j)) {
                occ |= (1ULL << bits[j]);
            }
        }
        result[i] = occ;
    }

    return result;
}
```

### Step 3: Calcolare Attacks (Ground Truth)

Implementa calcolo classico degli attacks per validazione:

```cpp
namespace pieces {

uint64_t calculateRookAttacksClassical(chess::Coords square, uint64_t occupancy) noexcept {
    uint64_t attacks = 0ULL;
    int8_t file = square.file();
    int8_t rank = square.rank();

    // Nord
    for (int8_t r = rank - 1; r >= 0; --r) {
        attacks |= (1ULL << (r * 8 + file));
        if (occupancy & (1ULL << (r * 8 + file))) break;
    }
    // Sud
    for (int8_t r = rank + 1; r < 8; ++r) {
        attacks |= (1ULL << (r * 8 + file));
        if (occupancy & (1ULL << (r * 8 + file))) break;
    }
    // Est
    for (int8_t f = file + 1; f < 8; ++f) {
        attacks |= (1ULL << (rank * 8 + f));
        if (occupancy & (1ULL << (rank * 8 + f))) break;
    }
    // Ovest
    for (int8_t f = file - 1; f >= 0; --f) {
        attacks |= (1ULL << (rank * 8 + f));
        if (occupancy & (1ULL << (rank * 8 + f))) break;
    }

    return attacks;
}

uint64_t calculateBishopAttacksClassical(chess::Coords square, uint64_t occupancy) noexcept {
    uint64_t attacks = 0ULL;
    int8_t file = square.file();
    int8_t rank = square.rank();

    // NE, NW, SE, SW (implementazione simile a Rook)
    // ... (dettagli implementazione)

    return attacks;
}

} // namespace pieces
```

### Step 4: Estrarre Magic Numbers da Stockfish

**NOTA**: Non generiamo magic numbers da zero. Usiamo quelli già verificati di Stockfish.

**Fonte Stockfish**:
- https://github.com/official-stockfish/Stockfish/blob/master/src/bitboard.cpp
- Cerca `RookMagics[]` e `BishopMagics[]`

**Processo**:
1. Scarica o consulta il codice sorgente di Stockfish
2. Trova gli array `RookMagics[64]` e `BishopMagics[64]`
3. Copia i 128 valori uint64_t (64 Rook + 64 Bishop)
4. Formatta per il nostro file `magic_numbers.hpp`

**Esempio dal codice Stockfish** (estratto):
```cpp
// Da Stockfish bitboard.cpp
const uint64_t RookMagics[64] = {
    0x0080001020400080ULL, 0x0040001000200040ULL, 0x0080081000200080ULL,
    // ... altri 61 valori ...
};

const uint64_t BishopMagics[64] = {
    0x0002020202020200ULL, 0x0002020202020000ULL, 0x0004010202000000ULL,
    // ... altri 61 valori ...
};
```

**Cosa faremo** (Step successivo):
Creeremo `magic_numbers.hpp` con questi valori hardcoded come:
```cpp
constexpr uint64_t ROOK_MAGICS[64] = { /* valori da Stockfish */ };
constexpr uint64_t BISHOP_MAGICS[64] = { /* valori da Stockfish */ };
```

### Step 5: Inizializzare Attack Tables

**Funzioni Helper**:

```cpp
namespace pieces {

// Helper: Popola attack table usando MagicData (Rook)
void populateRookAttackTable(chess::Coords square, const pieces::MagicData& magicInfo) noexcept {
    uint64_t mask = magicInfo.relevantSquares;
    int bitCount = __builtin_popcountll(mask);
    int numPatterns = 1 << bitCount;

    for (int i = 0; i < numPatterns; ++i) {
        uint64_t occupancy = pieces::generateOccupancyPattern(i, bitCount, mask);
        uint32_t index = magicInfo.computeIndex(occupancy);
        uint64_t attacks = pieces::calculateRookAttacksClassical(square, occupancy);
        magicInfo.attackTable[index] = attacks;
    }
}

// Helper: Popola attack table usando MagicData (Bishop)
void populateBishopAttackTable(chess::Coords square, const pieces::MagicData& magicInfo) noexcept {
    uint64_t mask = magicInfo.relevantSquares;
    int bitCount = __builtin_popcountll(mask);
    int numPatterns = 1 << bitCount;

    for (int i = 0; i < numPatterns; ++i) {
        uint64_t occupancy = pieces::generateOccupancyPattern(i, bitCount, mask);
        uint32_t index = magicInfo.computeIndex(occupancy);
        uint64_t attacks = pieces::calculateBishopAttacksClassical(square, occupancy);
        magicInfo.attackTable[index] = attacks;
    }
}

// Helper: Inizializza magic info per una square (Rook)
void initRookMagicForSquare(int sq, uint64_t*& tablePtr) noexcept {
    pieces::ROOK_MAGIC_INFO[sq].initialize(
        pieces::ROOK_MASKS[sq],
        pieces::ROOK_MAGICS[sq],
        tablePtr
    );

    pieces::populateRookAttackTable(chess::Coords(sq), pieces::ROOK_MAGIC_INFO[sq]);

    int bitCount = __builtin_popcountll(pieces::ROOK_MASKS[sq]);
    tablePtr += (1 << bitCount);
}

// Helper: Inizializza magic info per una square (Bishop)
void initBishopMagicForSquare(int sq, uint64_t*& tablePtr) noexcept {
    pieces::BISHOP_MAGIC_INFO[sq].initialize(
        pieces::BISHOP_MASKS[sq],
        pieces::BISHOP_MAGICS[sq],
        tablePtr
    );

    pieces::populateBishopAttackTable(chess::Coords(sq), pieces::BISHOP_MAGIC_INFO[sq]);

    int bitCount = __builtin_popcountll(pieces::BISHOP_MASKS[sq]);
    tablePtr += (1 << bitCount);
}

} // namespace pieces
```

**Funzione Principale** (molto più chiara!):

```cpp
namespace pieces {

void initMagicBitboards() noexcept {
    uint64_t* rookTablePtr = pieces::ROOK_ATTACK_LOOKUP.data();
    uint64_t* bishopTablePtr = pieces::BISHOP_ATTACK_LOOKUP.data();

    for (int sq = 0; sq < 64; ++sq) {
        pieces::initRookMagicForSquare(sq, rookTablePtr);
        pieces::initBishopMagicForSquare(sq, bishopTablePtr);
    }
}

} // namespace pieces
```

---

## 5. Organizzazione File

### ✅ DECISIONE: File Separato Header-Only

**Struttura scelta**:
```
piece/
├── piece.hpp              # Logica: struct, init, lookup functions
├── magic_numbers.hpp      # NUOVO: Dati hardcoded (magic numbers + masks)
└── test/
```

**`magic_numbers.hpp` conterrà**:
- `ROOK_MAGICS[64]` - Magic numbers per Rook
- `BISHOP_MAGICS[64]` - Magic numbers per Bishop
- `ROOK_MASKS[64]` - Relevant occupancy masks per Rook (hardcoded)
- `BISHOP_MASKS[64]` - Relevant occupancy masks per Bishop (hardcoded)
- Tutti valori `constexpr uint64_t`

**`piece.hpp` conterrà**:
- `#include "magic_numbers.hpp"`
- Struct `MagicData`
- Attack lookup tables: `ROOK_ATTACK_LOOKUP`, `BISHOP_ATTACK_LOOKUP`
- Magic info arrays: `ROOK_MAGIC_INFO`, `BISHOP_MAGIC_INFO`
- Funzione `initMagicBitboards()` - popola attack lookup tables
- Funzioni lookup: `getRookAttacks()`, `getBishopAttacks()`

**Vantaggi**:
- ✅ Separazione dati (magic_numbers.hpp) vs logica (piece.hpp)
- ✅ Compilazione veloce (modifiche a piece.hpp non toccano magic numbers)
- ✅ Manutenibilità (rigenerare magic → modifica solo 1 file)
- ✅ Leggibilità (piece.hpp rimane pulito ~400 righe)
- ✅ Zero overhead runtime (tutto hardcoded o pre-calcolato)

---

## 6. Strategia di Implementazione

### ✅ DECISIONE: Tutto Hardcoded, Init Runtime Solo per Attack Tables

**Dati Hardcoded** (in `magic_numbers.hpp`):
- ✅ Magic numbers per Rook (64 valori)
- ✅ Magic numbers per Bishop (64 valori)
- ✅ Relevant masks per Rook (64 valori)
- ✅ Relevant masks per Bishop (64 valori)

**Calcolato a Runtime** (in `initMagicBitboards()`):
- Attack tables popolate all'avvio
- Tempo stimato: <100ms
- Usa magic numbers hardcoded per popolare le tabelle

**Workflow**:
1. Genera magic numbers una volta con programma standalone
2. Copia valori in `magic_numbers.hpp`
3. All'avvio: `initMagicBitboards()` popola attack tables
4. Lookup O(1) durante il gioco

**Vantaggi**:
- Zero generazione random a runtime (deterministico)
- Startup veloce (<100ms vs 1-10 secondi)
- Magic numbers verificati offline
- Possibilità di ottimizzare magic numbers manualmente

---

## 7. Testing

```cpp
void testMagicBitboards() {
    // Test 1: Confronto con ray-based per tutti i pattern
    for (int sq = 0; sq < 64; sq++) {
        uint64_t mask = rookRelevantMask(sq);
        auto patterns = generateOccupancyPatterns(mask);

        for (uint64_t occ : patterns) {
            uint64_t magicResult = getRookAttacks(sq, occ);
            uint64_t rayResult = calculateRookAttacksClassical(sq, occ);
            assert(magicResult == rayResult);
        }
    }

    // Test 2: Posizioni note
    // A1 rook, board vuoto: dovrebbe attaccare tutta rank 1 + file A
    uint64_t a1_empty = getRookAttacks(0, 0);
    assert(a1_empty == 0x01010101010101FEULL);
}
```

---

## 8. Roadmap

### ✅ Fase 1: Estrazione Magic Numbers da Stockfish
- [ ] Scarica/consulta Stockfish source code (bitboard.cpp/h)
- [ ] Estrai magic numbers per Rook: `RookMagics[64]`
- [ ] Estrai magic numbers per Bishop: `BishopMagics[64]`
- [ ] Implementa `rookRelevantMask()` e `bishopRelevantMask()` per calcolare masks
- [ ] Calcola masks per tutte le 64 square (Rook + Bishop)
- [ ] Formatta output in formato C++ array per `magic_numbers.hpp`
- [ ] Opzionale: Crea script/programma per automatizzare estrazione

### Fase 2: Creazione `magic_numbers.hpp`
- [ ] Crea file `piece/magic_numbers.hpp`
- [ ] Copia magic numbers generati: `ROOK_MAGICS[64]`
- [ ] Copia magic numbers generati: `BISHOP_MAGICS[64]`
- [ ] Copia masks hardcoded: `ROOK_MASKS[64]`
- [ ] Copia masks hardcoded: `BISHOP_MASKS[64]`
- [ ] Aggiungi commenti e documentazione

### Fase 3: Implementazione in `piece.hpp`
- [ ] Aggiungi `#include "magic_numbers.hpp"`
- [ ] Definisci struct `MagicData`
- [ ] Dichiara attack tables: `ROOK_ATTACK_TABLE[]`, `BISHOP_ATTACK_TABLE[]`
- [ ] Implementa `initMagicBitboards()` - popola attack tables
- [ ] Implementa nuove `getRookAttacks()` e `getBishopAttacks()` con magic
- [ ] **Mantieni vecchio codice ray() temporaneamente per testing**

### Fase 4: Integrazione e Testing
- [ ] Modifica Engine constructor: aggiungi init con flag statico
- [ ] Crea test: confronta magic vs ray per tutte le configurazioni
- [ ] Test su posizioni note (A1 vuoto, E4 con blockers, etc.)
- [ ] Verifica correttezza in `generateLegalMoves()`
- [ ] Run test suite completa del progetto
- [ ] Fix eventuali bug

### Fase 5: Benchmark e Cleanup
- [ ] Benchmark isolato: magic vs ray (cicli CPU, throughput)
- [ ] Benchmark engine: search depth 6 su posizioni standard
- [ ] Misura speedup totale
- [ ] **Se tutto OK**: Rimuovi codice ray-based
- [ ] Rimuovi `RAY_MASK` e funzione `ray()`
- [ ] Update `getQueenAttacks()` (già usa Rook + Bishop)
- [ ] **Migliora `getPawnForwardPushes()`**:
  - [ ] Cambia signature: `chess::Coords square` invece di `int8_t squareIndex`
  - [ ] Usa `square.rank()` e `square.file()` internamente
  - [ ] Update chiamate a questa funzione nel codice
- [ ] Update documentazione (CLAUDE.md, commenti)
- [ ] Commit finale con messaggio descrittivo

---

## 9. Decisioni Prese ✅

1. **✅ Approccio per magic numbers**: Hardcodare tutto
   - Magic numbers generati offline e hardcoded
   - Masks hardcoded (non constexpr functions)
   - Attack tables popolate a runtime in `initMagicBitboards()`

2. **✅ Organizzazione file**: File separato `magic_numbers.hpp`
   - `magic_numbers.hpp`: dati hardcoded (magic + masks)
   - `piece.hpp`: logica (struct, init, lookup)

3. **✅ Backward compatibility**: Mantenere ray() durante testing
   - Fase 3-4: Codice ray() coesiste con magic
   - Fase 5: Rimuovere ray() dopo testing completo

4. **✅ Testing strategy**: Test esaustivo prima di rimuovere ray()
   - Test su tutte le 2^N configurazioni per ogni square
   - Confronto magic vs ray
   - Test suite completa del progetto

5. **✅ Benchmarking**: Doppio benchmark
   - Isolato: `getRookAttacks()` cicli CPU
   - Engine: search depth 6 su posizioni standard

---

## 10. Domande Rimanenti

1. **✅ DECISO: Generazione magic numbers**:
   - ~~Opzione A: Generare da zero con nostro tool~~
   - **✅ Opzione B: Usare magic da Stockfish** (più veloce, già verificati)
   - Fonte: https://github.com/official-stockfish/Stockfish/blob/master/src/bitboard.cpp

2. **✅ DECISO: Dove chiamare `initMagicBitboards()`**:
   - ~~Opzione A: In `main.cpp` all'inizio~~
   - **✅ Opzione B: In Engine constructor con flag statico**
   - ~~Opzione C: Static initialization (automatic)~~
   - **Implementazione**: Flag statico per init una volta sola

3. **✅ DECISO: Implementazione**:
   - ~~Opzione A: Implementare tutto insieme~~
   - **✅ Opzione B: Passo-passo con review intermedia dopo ogni fase**
   - **Workflow**: Completo una fase → review → feedback → fase successiva

---

## 11. Tutte le Decisioni Prese - Riepilogo

✅ **Magic numbers**: Da Stockfish
✅ **File organization**: `magic_numbers.hpp` separato (magic + masks hardcoded)
✅ **Init location**: Engine constructor con flag statico
✅ **Implementazione**: Passo-passo con review

**Pronti per iniziare la Fase 1!**
