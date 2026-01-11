# Transposition Table - Piano di Refactoring

**Data**: 2026-01-11
**Autore**: Progettazione collaborativa con Claude Code
**Obiettivi**:
1. Ristrutturare `tt.hpp` e `tt_helpers.cpp` per migliorare modularità e manutenibilità
2. Trasformare struct + funzioni globali in classe singleton OOP
3. **Separazione netta**: TT espone API con overload, gestisce conversioni internamente
4. Eliminare completamente `tt_helpers.cpp` (file bridge), Engine usa TT direttamente

**REGOLA IMPORTANTE**: Questo documento non deve contenere stime temporali. Focus su cosa va fatto, non su quanto tempo richiede.

---

## 1. Analisi Stato Attuale

### File Esistenti

**`tt.hpp`** (270 righe):
- Strutture dati: `TTEntry`, `TTGlobal`
- Zobrist hashing: `XorShift64`, `ZobristTables`, `makeZobristTables()`, `computeHashKey()`
- Gestione TT globale: `globalTTData()`, `globalTT()`, funzioni generation
- Operazioni TT: `prefetchTT()`, `storeTTEntry()`, `probeTT()`

**`tt_helpers.cpp`** (47 righe):
- Wrapper methods per Engine: `probeTTCache()`, `saveTTEntry()`

### Problemi Identificati

1. **Responsabilità multiple in un singolo file**: `tt.hpp` contiene strutture dati, hashing Zobrist, gestione memoria globale e algoritmi TT
2. **File header troppo grande**: 270 righe rendono difficile navigazione e manutenzione
3. **Accoppiamento**: Zobrist hashing strettamente legato alle operazioni TT
4. **Naming inconsistente**: `tt_helpers.cpp` contiene solo metodi Engine, non "helpers generici"
5. **Difficile testabilità**: funzioni globali e singleton rendono unit testing complesso
6. **Conversioni tramite file bridge**: Engine usa `int64_t` per score, TT usa `int32_t`, `tt_helpers.cpp` fa conversioni continue invece di gestirle internamente in TT

---

## 2. Principi di Design

### 2.1 Separazione delle Responsabilità

Ogni file deve avere una singola responsabilità chiara:
- **TranspositionTable**: classe singleton che incapsula dati e operazioni TT
- **Zobrist**: generazione chiavi hash indipendente dalla TT
- **Engine integration**: wrapper specifici per Engine

### 2.2 Modularità

Ogni componente deve essere utilizzabile indipendentemente:
- Zobrist hashing deve funzionare senza TT
- TranspositionTable: interfaccia chiara e incapsulata
- Engine helpers devono essere chiaramente separati dalle primitive TT

### 2.3 Encapsulation e OOP

- TranspositionTable come classe singleton invece di funzioni globali + struct
- Entry come nested type della classe (scope limitato)
- Metodi pubblici chiari: `probe()`, `store()`, `prefetch()`
- Dati privati: array di entry, generation counter

### 2.4 Testabilità

- Interfaccia ben definita per testing
- Possibilità di aggiungere mock/test variant se necessario
- Zobrist completamente indipendente e testabile

---

## 3. Struttura Proposta

### 3.1 Nuova Organizzazione File

```
tt/
├── zobrist.hpp              # Zobrist hashing con std::array
└── transposition_table.hpp  # TranspositionTable class (include zobrist.hpp)

engine/
├── engine.hpp               # Include tt/transposition_table.hpp
├── engine.cpp               # Aggiornato per usare TranspositionTable::instance()
└── search.cpp               # Usa TT direttamente (no file bridge)
```

**Decisioni di design**:
- ✅ Directory: `tt/` al livello root (come `board/`, `piece/`)
- ✅ NO backward compatibility: aggiornamento diretto di tutto il codice
- ✅ Zobrist separato in file dedicato: `zobrist.hpp`
- ✅ TranspositionTable in file dedicato: `transposition_table.hpp` (include zobrist.hpp)
- ✅ Naming: `probe()`, `store()` (semplice e chiaro)
- ✅ Due header separati per responsabilità chiare

**Separazione responsabilità**:
- **2 file header** invece di 1 monolitico (zobrist.hpp + transposition_table.hpp)
- Zobrist e TT fisicamente separati e logicamente separati (namespaces + file diversi)
- No wrapper deprecati, fix diretti al codice esistente
- Include chain: transposition_table.hpp → zobrist.hpp → board.hpp

### 3.2 Descrizione Dettagliata dei Moduli

#### `tt/zobrist.hpp` (~140 righe)

**Struttura del file**:
```cpp
#ifndef TT_ZOBRIST_HPP
#define TT_ZOBRIST_HPP

#include <cstdint>
#include <cstddef>
#include <array>
#include "../board/board.hpp"

namespace zobrist {
    // RNG per compile-time generation
    struct XorShift64 {
        uint64_t state;
        constexpr explicit XorShift64(uint64_t seed) : state(seed) {}
        constexpr uint64_t next() {
            uint64_t x = state;
            x ^= x >> 12;
            x ^= x << 25;
            x ^= x >> 27;
            state = x;
            return x * 0x2545F4914F6CDD1DULL;
        }
    };

    // Costanti per dimensioni array
    constexpr std::size_t PIECE_TYPES = 16;    // 16 tipi di pezzo (0=empty, 1-6=white, 9-14=black)
    constexpr std::size_t SQUARES = 64;         // 64 caselle
    constexpr std::size_t CASTLING_STATES = 16; // 16 stati castling (bitmask KQkq: 0-15)
    constexpr std::size_t FILES = 8;            // 8 colonne per en-passant

    // Tabelle Zobrist con std::array
    struct Tables {
        std::array<std::array<uint64_t, SQUARES>, PIECE_TYPES> pieces;
        uint64_t sideToMove;
        std::array<uint64_t, CASTLING_STATES> castling;
        std::array<uint64_t, FILES> enPassant;
    };

    // Generazione compile-time
    constexpr Tables makeTables() {
        Tables t{};
        XorShift64 rng(0x123456789ABCDEF0ULL);

        // Pezzi: 16 tipi × 64 caselle
        for (std::size_t pieceType = 0; pieceType < PIECE_TYPES; ++pieceType) {
            for (std::size_t square = 0; square < SQUARES; ++square) {
                t.pieces[pieceType][square] = rng.next();
            }
        }

        // Side to move
        t.sideToMove = rng.next();

        // Castling: 16 stati possibili
        for (std::size_t i = 0; i < CASTLING_STATES; ++i) {
            t.castling[i] = rng.next();
        }

        // En-passant: 8 colonne
        for (std::size_t file = 0; file < FILES; ++file) {
            t.enPassant[file] = rng.next();
        }

        return t;
    }

    // Tabelle globali compile-time
    inline constexpr Tables TABLES = makeTables();

    // Helper per XOR pezzi da bitboard (più leggibile e riusabile)
    inline void xorPiecesFromBitboard(uint64_t& hashKey, uint64_t bitboard, std::size_t pieceIndex) {
        while (bitboard) {
            const uint8_t square = static_cast<uint8_t>(__builtin_ctzll(bitboard));
            bitboard &= (bitboard - 1);
            hashKey ^= TABLES.pieces[pieceIndex][square];
        }
    }

    // Calcolo chiave hash da Board
    inline uint64_t computeHashKey(const chess::Board& board) {
        uint64_t hashKey = 0ULL;

        // White pieces (piece index 1-6)
        xorPiecesFromBitboard(hashKey, board.pawns_bb[0],   1);
        xorPiecesFromBitboard(hashKey, board.knights_bb[0], 2);
        xorPiecesFromBitboard(hashKey, board.bishops_bb[0], 3);
        xorPiecesFromBitboard(hashKey, board.rooks_bb[0],   4);
        xorPiecesFromBitboard(hashKey, board.queens_bb[0],  5);
        xorPiecesFromBitboard(hashKey, board.kings_bb[0],   6);

        // Black pieces (piece index 9-14)
        xorPiecesFromBitboard(hashKey, board.pawns_bb[1],   9);
        xorPiecesFromBitboard(hashKey, board.knights_bb[1], 10);
        xorPiecesFromBitboard(hashKey, board.bishops_bb[1], 11);
        xorPiecesFromBitboard(hashKey, board.rooks_bb[1],   12);
        xorPiecesFromBitboard(hashKey, board.queens_bb[1],  13);
        xorPiecesFromBitboard(hashKey, board.kings_bb[1],  14);

        // Side to move (branchless: XOR if black to move)
        const uint64_t stmMask = static_cast<uint64_t>(-(board.getActiveColor() == chess::Board::BLACK));
        hashKey ^= TABLES.sideToMove & stmMask;

        // Castling rights (0-15 bitmask)
        const uint8_t castlingMask =
            (board.getCastle(0) ? 1u : 0u) |
            (board.getCastle(1) ? 2u : 0u) |
            (board.getCastle(2) ? 4u : 0u) |
            (board.getCastle(3) ? 8u : 0u);
        hashKey ^= TABLES.castling[castlingMask];

        // En-passant (branchless: XOR if valid EP square)
        const chess::Coords epSquare = board.getEnPassant();
        const uint64_t epMask = static_cast<uint64_t>(-static_cast<int64_t>(chess::Coords::isInBounds(epSquare)));
        hashKey ^= TABLES.enPassant[epSquare.file()] & epMask;

        return hashKey;
    }
}

#endif // TT_ZOBRIST_HPP
```

**Responsabilità**:
- Generazione compile-time delle tabelle Zobrist con `std::array`
- Calcolo efficiente delle chiavi hash da posizioni scacchistiche
- Helper function `xorPiecesFromBitboard()` per riusabilità

**Vantaggi**:
- **File indipendente**: Può essere usato senza TranspositionTable
- **Compile-time generation**: Zero overhead runtime
- **Type-safe**: `std::array` con costanti esplicite
- **Testabile**: Può essere testato in isolamento

---

#### `tt/transposition_table.hpp` (~210 righe)

**Struttura del file**:
```cpp
#ifndef TT_TRANSPOSITION_TABLE_HPP
#define TT_TRANSPOSITION_TABLE_HPP

#include <cstdint>
#include <cstddef>
#include <array>
#include <algorithm>
#include "zobrist.hpp"

namespace tt {
    class TranspositionTable {
    public:
        // Nested Entry type
        struct Entry {
            uint64_t key;       // [0-7]   Hash key for position
            int32_t  score;     // [8-11]  Evaluation score
            uint8_t  depth;     // [12]    Search depth
            uint8_t  age;       // [13]    Generation counter
            uint8_t  flag;      // [14]    Entry type (EXACT/LOWERBOUND/UPPERBOUND)
            uint8_t  padding;   // [15]    Alignment padding
            
            // Total: 16 bytes (cache-line friendly, 4 entries = 64 bytes = 1 cache line)

            enum Flag : uint8_t {
                INVALID = 0,
                EXACT,
                LOWERBOUND,
                UPPERBOUND
            };
        };

        // Configurazione
        static constexpr std::size_t BUCKET_COUNT = 1u << 20;
        static constexpr std::size_t ENTRIES_PER_BUCKET = 4;
        static constexpr std::size_t TABLE_SIZE = BUCKET_COUNT * ENTRIES_PER_BUCKET;
        static constexpr int32_t ADJUSTMENT = 50;

        // Validazioni compile-time
        static_assert(sizeof(Entry) == 16, "Entry must be exactly 16 bytes");
        static_assert(alignof(Entry) == 8, "Entry must be 8-byte aligned for cache efficiency");
        static_assert((BUCKET_COUNT & (BUCKET_COUNT - 1)) == 0, "BUCKET_COUNT must be power of 2");

        // Costruttore pubblico (per istanza in Engine)
        TranspositionTable() = default;

        // Operazioni principali con int32_t (native API)
        inline void prefetch(uint64_t key) noexcept;
        inline bool probe(uint64_t key, uint8_t depth, int32_t alpha, int32_t beta, int32_t& outScore) noexcept;
        inline void store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag) noexcept;

        // Overload per int64_t (conversioni gestite internamente)
        inline bool probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta, int64_t& outScore) noexcept;
        inline void store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag) noexcept;

        // Generation management
        void incrementGeneration() { ++generation_; }
        uint8_t getCurrentGeneration() const { return generation_; }

        // Utility
        void clear();
        size_t getMemoryUsage() const { return sizeof(table_); }

        // Prevent copying, allow moving
        TranspositionTable(const TranspositionTable&) = delete;
        TranspositionTable& operator=(const TranspositionTable&) = delete;
        TranspositionTable(TranspositionTable&&) = default;
        TranspositionTable& operator=(TranspositionTable&&) = default;

    private:
        std::array<Entry, TABLE_SIZE> table_;
        uint8_t generation_ = 0;
    };

    // Implementazioni inline dei metodi critici
    inline void TranspositionTable::prefetch(uint64_t key) noexcept {
        const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
        const Entry* bucket = &table_[bucketIndex * ENTRIES_PER_BUCKET];
        
        // Prefetch for read (0), high temporal locality (3)
        // Brings 64-byte cache line (4 entries) into L1 cache
        // ~200 cycle latency reduction on cache miss
        __builtin_prefetch(bucket, 0, 3);
    }

    inline bool TranspositionTable::probe(uint64_t key, uint8_t depth,
                                          int32_t alpha, int32_t beta, int32_t& outScore) noexcept {
        const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
        const Entry* bucket = &table_[bucketIndex * ENTRIES_PER_BUCKET];

        for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
            const Entry& entry = bucket[i];

            if (entry.flag == Entry::INVALID) continue;
            if (entry.key != key) continue;
            if (entry.depth < depth) continue;

            const int32_t score = entry.score;

            switch (entry.flag) {
                case Entry::EXACT:
                    outScore = score;
                    return true;
                case Entry::LOWERBOUND:
                    if (score >= beta) {
                        outScore = score;
                        return true;
                    }
                    break;
                case Entry::UPPERBOUND:
                    if (score <= alpha) {
                        outScore = score;
                        return true;
                    }
                    break;
            }

            return false;
        }

        return false;
    }

    inline void TranspositionTable::store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag) noexcept {
        const std::size_t bucketIndex = static_cast<std::size_t>(key) & (BUCKET_COUNT - 1);
        Entry* bucket = &table_[bucketIndex * ENTRIES_PER_BUCKET];

        Entry* replaceEntry = &bucket[0];
        int bestReplaceScore = -1000000;

        for (std::size_t i = 0; i < ENTRIES_PER_BUCKET; ++i) {
            Entry& entry = bucket[i];

            if (entry.key == key) {
                if (depth >= entry.depth || flag == Entry::EXACT) {
                    entry.depth = depth;
                    entry.score = score;
                    entry.flag  = flag;
                    entry.age   = generation_;
                }
                return;
            }

            const int ageDiff = static_cast<int>(generation_ - entry.age) & 0xFF;
            const int replaceScore = (ageDiff * 256) - static_cast<int>(entry.depth) * 4;

            if (replaceScore > bestReplaceScore) {
                bestReplaceScore = replaceScore;
                replaceEntry = &entry;
            }
        }

        replaceEntry->key = key;
        replaceEntry->depth = depth;
        replaceEntry->score = score;
        replaceEntry->flag  = flag;
        replaceEntry->age   = generation_;
    }

    // Overload int64_t → int32_t (conversioni gestite da TT)
    inline bool TranspositionTable::probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta, int64_t& outScore) noexcept {
        const int32_t alpha32 = static_cast<int32_t>(
            std::max<int64_t>(alpha - ADJUSTMENT, INT32_MIN + 1));
        const int32_t beta32 = static_cast<int32_t>(
            std::min<int64_t>(beta + ADJUSTMENT, INT32_MAX - 1));

        int32_t score32 = 0;
        if (probe(key, depth, alpha32, beta32, score32)) {
            outScore = static_cast<int64_t>(score32);
            return true;
        }
        return false;
    }

    inline void TranspositionTable::store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag) noexcept {
        const int32_t score32 = static_cast<int32_t>(
            std::max<int64_t>(std::min<int64_t>(score, INT32_MAX - 1), INT32_MIN + 1));
        store(key, depth, score32, flag);
    }

    inline void TranspositionTable::clear() {
        for (Entry& entry : table_) {
            entry.flag = Entry::INVALID;
        }
    }

    // Helper per determinare flag TT (riduce duplicazione codice in Engine)
    inline constexpr TranspositionTable::Entry::Flag 
    determineFlag(int64_t score, int64_t alphaOrig, int64_t beta) noexcept {
        if (score <= alphaOrig) return TranspositionTable::Entry::UPPERBOUND;
        if (score >= beta) return TranspositionTable::Entry::LOWERBOUND;
        return TranspositionTable::Entry::EXACT;
    }
}

#endif // TT_TRANSPOSITION_TABLE_HPP
```

**Responsabilità**:
- Classe `TranspositionTable` con operazioni TT
- **Costruttore pubblico**: Permette istanza membro in Engine
- Gestione memory, generation counter, replacement policy
- Overload int64_t per integrazione trasparente con Engine
- Helper `determineFlag()` per calcolo flag TT

**Vantaggi**:
- **Include zobrist.hpp**: Dependency esplicita e chiara
- **Classe OOP completa**: Encapsulation, API pulita
- **Costruttore pubblico**: Engine può avere `tt::TranspositionTable tt;` come membro
- **std::array**: Type-safe, range-based for in `clear()`
- **Inline methods con noexcept**: Zero overhead, ottimizzazioni aggressive
- **Validazioni compile-time**: `static_assert` per Entry layout
- **Commenti espliciti**: Documenta memory layout e prefetch behavior
- **Move semantics**: Permette move (anche se non necessario con istanza membro)

**Differenze chiave rispetto a design con struct**:
- Un file invece di 2 (tt.hpp + zobrist come detail)
- Classe invece di struct + funzioni globali
- Namespace `zobrist` e `tt` invece di tutto in `engine`
- Nessun file bridge: Engine usa TT direttamente tramite overload

### 3.3 Separazione Netta: TT Espone API, Elimina File Bridge

**Decisione finale**: Eliminare `tt_helpers.cpp`, TT espone API con overload per gestire conversioni internamente.

#### Problema Attuale

```cpp
// Engine usa int64_t
struct AlphaBeta {
    int64_t alpha;
    int64_t beta;
};

// TT usa int32_t internamente
struct TTEntry {
    int32_t score;
    ...
};

// tt_helpers.cpp fa da BRIDGE con conversioni
bool Engine::probeTTCache(...) {
    // Conversioni int64_t → int32_t
    const int32_t alpha16 = static_cast<int32_t>(bounds.alpha - TTEntry::ADJUSTMENT);
    ...
    if (probeTT(this->ttTable, hashKey, depth, alpha16, beta16, ttScore)) {
        score = static_cast<int64_t>(ttScore);  // Conversione int32_t → int64_t
        return true;
    }
}
```

**Problema**: File bridge inutile che rompe la separazione tra TT e Engine.

#### Soluzione: TT Gestisce Conversioni con Overload

**Principio**: TT deve esporre un'API che nasconde i dettagli implementativi (usa int32_t internamente, ma accetta int64_t per comodità).

**Perché int32_t è perfetto per gli scacchi?**
- Range: ±2,147,483,647 (±2 miliardi)
- Mate score tipico: ±30,000
- Evaluation tipica: ±5,000
- **Zero rischio overflow**: impossibile superare range con valutazioni scacchistiche realistiche
- **Stesso memory footprint**: Entry rimane 16 bytes (padding si riduce da 3 a 1 byte)
- **Performance**: int32_t è nativo su CPU moderne (register width, ALU operations)

```cpp
// tt/transposition_table.hpp
namespace tt {
    class TranspositionTable {
    public:
        // API nativa con int32_t (usata internamente)
        inline bool probe(uint64_t key, uint8_t depth, int32_t alpha, int32_t beta, int32_t& outScore);
        inline void store(uint64_t key, uint8_t depth, int32_t score, uint8_t flag);

        // Overload per int64_t - CONVERSIONI GESTITE DA TT
        inline bool probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta, int64_t& outScore) {
            // Conversione int64_t → int32_t (responsabilità di TT)
            const int32_t alpha16 = static_cast<int32_t>(
                std::max<int64_t>(alpha - ADJUSTMENT, INT32_MIN + 1));
            const int32_t beta16 = static_cast<int32_t>(
                std::min<int64_t>(beta + ADJUSTMENT, INT32_MAX - 1));

            int32_t score16 = 0;
            if (probe(key, depth, alpha16, beta16, score16)) {
                outScore = static_cast<int64_t>(score16);  // Conversione int32_t → int64_t
                return true;
            }
            return false;
        }

        inline void store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag) {
            // Conversione int64_t → int32_t con clamping
            const int32_t score16 = static_cast<int32_t>(
                std::max<int64_t>(
                    std::min<int64_t>(score, INT32_MAX - 1),
                    INT32_MIN + 1));
            store(key, depth, score16, flag);
        }
    };
}
```

#### Engine Usa TT Direttamente

```cpp
// engine/engine.hpp
namespace engine {
    class Engine {
    private:
        tt::TranspositionTable tt;  // Istanza membro invece di singleton
        // ... altri membri ...
    };
}

// engine/search.cpp
int64_t score;

// Probe diretto tramite membro - NO .instance()
if (this->tt.probe(hashKey, depth, bounds.alpha, bounds.beta, score)) {
    return score;
}

// Store diretto tramite membro - NO .instance()
uint8_t flag = (best <= alphaOrig) ? tt::TranspositionTable::Entry::UPPERBOUND :
               (best >= bounds.beta) ? tt::TranspositionTable::Entry::LOWERBOUND :
                                       tt::TranspositionTable::Entry::EXACT;
this->tt.store(hashKey, depth, best, flag);
```

#### Vantaggi Soluzione

1. ✅ **Elimina tt_helpers.cpp completamente**: Nessun file bridge
2. ✅ **Separazione netta**: TT gestisce le sue conversioni, Engine non sa che TT usa int32_t
3. ✅ **Engine invariato**: Continua a usare int64_t (no refactor massiccio)
4. ✅ **API pulita**: Overload trasparente, conversioni nascoste
5. ✅ **Type safety**: Overload risolto a compile-time
6. ✅ **Zero overhead**: Inline, conversioni compilate via
7. ✅ **Responsabilità corretta**: TT gestisce i suoi dettagli implementativi
8. ✅ **Accesso diretto**: `this->tt.probe()` invece di `tt::TranspositionTable::instance().probe()` (più conciso)

#### File da Modificare

1. **tt/zobrist.hpp**: Creare file nuovo con Zobrist hashing
2. **tt/transposition_table.hpp**: Creare file nuovo con classe TT (costruttore pubblico, no singleton)
3. **engine/engine.hpp**: Aggiungere membro `tt::TranspositionTable tt;`
4. **engine/search.cpp**: Usare `this->tt.probe()` e `this->tt.store()`
5. **Eliminare engine/tt_helpers.cpp**: Non più necessario

#### Confronto Chiamate

**PRIMA (con tt_helpers.cpp)**:
```cpp
if (this->probeTTCache(hashKey, depth, bounds, score)) {  // metodo wrapper
    return score;
}
TTSaveInfo info{hashKey, depth, best, alphaOrig, bounds.beta, 0};
this->saveTTEntry(info);  // metodo wrapper
```

**DOPO (istanza membro con overload)**:
```cpp
// Probe diretto tramite membro - conversioni gestite da overload
if (this->tt.probe(hashKey, depth, bounds.alpha, bounds.beta, score)) {
    return score;
}

// Store diretto tramite membro - usa helper determineFlag()
const auto flag = tt::determineFlag(best, alphaOrig, bounds.beta);
this->tt.store(hashKey, depth, best, flag);
```

#### Nessun Rischio

| Aspetto | Valutazione |
|---------|-------------|
| Performance | Identica: overload risolto a compile-time, inline |
| Correttezza | Conversioni identiche a prima, spostate in TT |
| Manutenibilità | Migliore: separazione netta, no file bridge |
| Testing | Invariato: stessa logica, diversa posizione |

---

## 4. Namespace Strategy

### Attuale (engine/tt.hpp)
```cpp
namespace engine {
    struct TTEntry { ... };
    struct TTGlobal { ... };
    inline TTGlobal& globalTTData() { ... }
    inline void prefetchTT(...) { ... }
    inline void storeTTEntry(...) { ... }
    inline bool probeTT(...) { ... }
    namespace detail {
        // Zobrist stuff
    }
    inline uint64_t computeHashKey(...) { ... }
    // ... tutto mischiato nel namespace engine ...
}
```

### Proposto (tt/transposition_table.hpp)
```cpp
// Zobrist hashing - namespace globale
namespace zobrist {
    struct XorShift64 { ... };
    struct ZobristTables { ... };
    constexpr ZobristTables makeZobristTables();
    inline constexpr ZobristTables ZOBRIST = makeZobristTables();
    inline uint64_t computeHashKey(const chess::Board&);
}

// Transposition Table - namespace globale
namespace tt {
    class TranspositionTable {
    public:
        struct Entry { ... };
        static TranspositionTable& instance();
        // ... metodi pubblici ...
    private:
        Entry table_[TABLE_SIZE];
        uint8_t generation_;
    };
}

// Engine methods (in engine.cpp / search.cpp - no file bridge)
namespace engine {
    // Usa tt::TranspositionTable e zobrist::computeHashKey direttamente
}
```

**Vantaggi**:
- **Namespace globali** `zobrist` e `tt` invece di sotto `engine`
  - Più puliti: `zobrist::computeHashKey()` vs `engine::zobrist::computeHashKey()`
  - Riutilizzabili: Zobrist non legato all'engine concettualmente
- **Separazione logica chiara**:
  - `zobrist::` per hashing
  - `tt::` per transposition table
  - `engine::` per engine-specific logic
- **Type scoping**: `tt::TranspositionTable::Entry` è self-documenting
- **Facilita auto-completion IDE**
- **Previene name collisions**

**Decisione finale**: Namespace globali `zobrist` e `tt` perché sono concetti generali (non specifici dell'engine), anche se usati principalmente dall'engine.

---

## 5. Migration Plan (Breaking Change Diretto)

**Strategia**: NO backward compatibility, aggiornamento diretto di tutto il codice in un'unica sessione.

### Step 1: Backup e Preparazione
```bash
cd /home/daniele/Documenti/chess_engine
# Backup file esistenti
cp engine/tt.hpp engine/tt.hpp.backup
cp engine/tt_helpers.cpp engine/tt_helpers.cpp.backup
# Creare nuova directory
mkdir tt
```

### Step 2: Creare tt/zobrist.hpp e tt/transposition_table.hpp
- **Step 2a: Creare tt/zobrist.hpp**
  - Implementare file completo (~140 righe):
    - `struct XorShift64` con `constexpr next()`
    - Costanti: `PIECE_TYPES`, `SQUARES`, `CASTLING_STATES`, `FILES`
    - `struct Tables` con `std::array`
    - `constexpr Tables makeTables()`
    - `inline constexpr Tables TABLES = makeTables()`
    - `inline void xorPiecesFromBitboard(...)` - helper function
    - `inline uint64_t computeHashKey(const chess::Board&)`

- **Step 2b: Creare tt/transposition_table.hpp**
  - Implementare file completo (~210 righe):
    - Include `"zobrist.hpp"`
    - `class TranspositionTable` con nested `Entry` e `enum Flag`
    - Costanti: `BUCKET_COUNT`, `ENTRIES_PER_BUCKET`, `TABLE_SIZE`, `ADJUSTMENT`
    - Singleton: `static TranspositionTable& instance()`
    - Metodi inline int32_t: `prefetch()`, `probe()`, `store()`
    - Overload inline int64_t: `probe()`, `store()`
    - Generation management, `clear()`, `getMemoryUsage()`
    - Private: `std::array<Entry, TABLE_SIZE> table_`, `uint8_t generation_`

### Step 3: Aggiornare engine/engine.hpp
- [ ] **Rimuovere include**: `#include "tt.hpp"`
- [ ] **Aggiungere include**: `#include "../tt/transposition_table.hpp"`
- [ ] **Aggiungere membro privato**: `tt::TranspositionTable tt;`
- [ ] **Rimuovere campo** (se presente): `TTEntry* ttTable;`
- [ ] **Rimuovere metodi**: `probeTTCache()` e `saveTTEntry()` declarations
- [ ] **Rimuovere struct**: `TTSaveInfo` (se presente)
- [ ] **Verificare**: Altri campi/metodi non dipendono da strutture TT

### Step 4: Aggiornare engine/engine.cpp
- [ ] **Include**: Già gestito tramite `engine.hpp`
- [ ] **Costruttore Engine**:
  - [ ] Rimuovere: `this->ttTable = engine::globalTT();` (se presente)
  - [ ] TT inizializzato automaticamente come membro
- [ ] **Metodo computeHashKey**:
  - [ ] Cambiare: `engine::computeHashKey(board)` → `zobrist::computeHashKey(board)`
- [ ] **Metodo prefetchTT** (se usato direttamente):
  - [ ] Cambiare: `prefetchTT(key)` → `this->tt.prefetch(key)`
- [ ] **Metodo incrementGeneration** (se usato):
  - [ ] Cambiare: `incrementTTGeneration()` → `this->tt.incrementGeneration()`
- [ ] **Cercare tutti gli usi**:
  ```bash
  grep -n "TTEntry\|globalTT\|prefetchTT\|storeTTEntry\|probeTT\|computeHashKey\|incrementTTGeneration" engine.cpp
  ```

### Step 5: Rimuovere file vecchi
```bash
# Dopo verifica che tutto compila
rm engine/tt.hpp
# Mantenere backup in caso di problemi
```

### Step 6: Aggiornare Makefile
- **Sorgenti**: Rimuovere `tt_helpers.cpp` dalla lista (file eliminato)
- **Include paths**: Verificare che `tt/` sia accessibile (dovrebbe funzionare con `../tt/`)
- **Se necessario**: Aggiungere `-I.` per include relativi

### Step 7: Testing Completo
```bash
# Compilazione
make cls && make prod
# Se ci sono errori, verificare:
# - Namespace qualifiers (tt::, zobrist::)
# - Include paths
# - Flag enum access (Entry::EXACT)

# Test funzionalità
make test && ./tests/outputTest

# Test integration
./chess
# Provare alcune mosse, verificare che l'engine funzioni
```

### Step 8: Verifiche Finali
- **sizeof(Entry)**: Deve essere 16 bytes
  ```cpp
  static_assert(sizeof(tt::TranspositionTable::Entry) == 16);
  ```
- **Memory usage**: Verificare che singleton allochi ~64MB
- **Assembly inspection**: Confrontare probe/store con versione precedente
- **Grep residui**:
  ```bash
  grep -r "TTEntry\|globalTT\|engine::computeHashKey" engine/ --include="*.cpp" --include="*.hpp"
  # Dovrebbe essere vuoto (tranne backup files)
  ```

---

## 6. Considerazioni Tecniche

### 6.1 Include Dependencies

**Ordine dipendenze semplificato**:
```
zobrist.hpp (include board.hpp)
    ↓
transposition_table.hpp (no dependencies oltre a <cstdint>)
    ↓
engine.hpp (include transposition_table.hpp, zobrist.hpp)
```

**Nota**: `transposition_table.hpp` e `zobrist.hpp` sono indipendenti (possono essere inclusi in qualsiasi ordine).

### 6.2 Singleton Pattern - Thread Safety

**Meyer's Singleton** (C++11 thread-safe):
```cpp
static TranspositionTable& instance() {
    static TranspositionTable instance;  // Thread-safe initialization
    return instance;
}
```

**Nota**: C++11 garantisce thread-safe initialization di static local variables (costruzione atomica con mutex interno del compilatore).

### 6.3 Memory Layout

**Entry size**: 16 bytes (invariato)
```
key:     8 bytes
score:   4 bytes (int32_t - più sicuro, nessun rischio overflow)
depth:   1 byte
age:     1 byte
flag:    1 byte
padding: 1 byte (per allineamento a 16 bytes)
Total:   16 bytes
```

**Tabella**: 4M entries × 16 bytes = 64 MB (invariato)

**Nota**: Passando da int16_t (2 bytes) a int32_t (4 bytes) per score, il padding si riduce da 3 a 1 byte. Entry totale rimane 16 bytes.

### 6.4 Backward Compatibility

**Strategia**:
- Trasformare `tt.hpp` in wrapper deprecato con `#warning`
- Fornire type aliases per codice legacy: `using TTEntry = TranspositionTable::Entry`
- Codice esistente continua a compilare con warning

---

## 7. Checklist Implementazione (Breaking Change)

### Preparazione
- [ ] Backup file esistenti
  ```bash
  cp engine/tt.hpp engine/tt.hpp.backup
  cp engine/tt_helpers.cpp engine/tt_helpers.cpp.backup  # Sarà eliminato completamente
  ```
- [ ] Creare directory: `mkdir tt`

### Implementazione tt/zobrist.hpp
- [ ] Creare file `tt/zobrist.hpp` (~140 righe)
- [ ] Header guards: `#ifndef TT_ZOBRIST_HPP`
- [ ] Includes: `<cstdint>`, `<cstddef>`, `<array>`, `"../board/board.hpp"`
- [ ] **Namespace zobrist**
  - [ ] `struct XorShift64` con `constexpr next()`
  - [ ] Costanti: `PIECE_TYPES`, `SQUARES`, `CASTLING_STATES`, `FILES`
  - [ ] `struct Tables` con `std::array` per pieces, castling, enPassant
  - [ ] `constexpr Tables makeTables()` - usa std::size_t nei loop
  - [ ] `inline constexpr Tables TABLES = makeTables()`
  - [ ] `inline void xorPiecesFromBitboard(...)` - helper function estratta per riusabilità
  - [ ] `inline uint64_t computeHashKey(const chess::Board&)` - usa helper function

### Implementazione tt/transposition_table.hpp
- [ ] Creare file `tt/transposition_table.hpp` (~220 righe)
- [ ] Header guards: `#ifndef TT_TRANSPOSITION_TABLE_HPP`
- [ ] Includes: `<cstdint>`, `<cstddef>`, `<array>`, `<algorithm>`, `"zobrist.hpp"`
- [ ] **Namespace tt**
  - [ ] `class TranspositionTable`
  - [ ] Nested `struct Entry` con commenti layout memory (`[0-7]`, `[8-11]`, ecc.)
  - [ ] `enum Flag` (INVALID, EXACT, LOWERBOUND, UPPERBOUND)
  - [ ] Costanti: `BUCKET_COUNT`, `ENTRIES_PER_BUCKET`, `TABLE_SIZE`, `ADJUSTMENT`
  - [ ] **static_assert**: `sizeof(Entry) == 16`, `alignof(Entry) == 8`, `BUCKET_COUNT` power of 2
  - [ ] **Costruttore pubblico**: `TranspositionTable() = default;` (NO singleton)
  - [ ] `inline void prefetch(uint64_t key) noexcept` - con commento dettagliato prefetch
  - [ ] `inline bool probe(uint64_t, uint8_t, int32_t, int32_t, int32_t&) noexcept` - versione int32_t
  - [ ] `inline void store(uint64_t, uint8_t, int32_t, uint8_t) noexcept` - versione int32_t
  - [ ] `inline bool probe(uint64_t, uint8_t, int64_t, int64_t, int64_t&) noexcept` - overload int64_t
  - [ ] `inline void store(uint64_t, uint8_t, int64_t, uint8_t) noexcept` - overload int64_t
  - [ ] `void incrementGeneration()`, `uint8_t getCurrentGeneration() const`
  - [ ] `void clear()` - usa range-based for loop su `std::array`
  - [ ] `size_t getMemoryUsage() const`
  - [ ] Delete copy constructor/assignment, default move constructor/assignment
  - [ ] Private: `std::array<Entry, TABLE_SIZE> table_`, `uint8_t generation_`
  - [ ] **Helper function**: `inline constexpr Entry::Flag determineFlag(int64_t, int64_t, int64_t) noexcept`

### Aggiungere Overload int64_t a TranspositionTable

**IMPORTANTE**: Aggiungiamo overload alla classe TT per gestire conversioni internamente.

#### Step 3a: Aggiungere overload in tt/transposition_table.hpp
- [ ] Dopo i metodi `probe()` e `store()` con int32_t, aggiungere overload int64_t:

  ```cpp
  // Overload per int64_t (conversioni gestite da TT)
  inline bool probe(uint64_t key, uint8_t depth, int64_t alpha, int64_t beta, int64_t& outScore) {
      const int32_t alpha16 = static_cast<int32_t>(
          std::max<int64_t>(alpha - ADJUSTMENT, INT32_MIN + 1));
      const int32_t beta16 = static_cast<int32_t>(
          std::min<int64_t>(beta + ADJUSTMENT, INT32_MAX - 1));

      int32_t score16 = 0;
      if (probe(key, depth, alpha16, beta16, score16)) {
          outScore = static_cast<int64_t>(score16);
          return true;
      }
      return false;
  }

  inline void store(uint64_t key, uint8_t depth, int64_t score, uint8_t flag) {
      const int32_t score16 = static_cast<int32_t>(
          std::max<int64_t>(std::min<int64_t>(score, INT32_MAX - 1), INT32_MIN + 1));
      store(key, depth, score16, flag);
  }
  ```

- [ ] Verificare: overload inline, conversioni identiche a quelle in tt_helpers.cpp

### Uso Diretto TT da Engine (Senza Bridge)

#### Step 3b: Aggiornare engine/engine.hpp
- [ ] Rimuovere dichiarazioni metodi:
  - [ ] `bool probeTTCache(...)`
  - [ ] `void saveTTEntry(...)`
- [ ] Rimuovere struct `TTSaveInfo` (non più necessario, parametri passati direttamente)
- [ ] Verificare: `AlphaBeta` struct rimane invariato (int64_t)

#### Step 3c: Uso diretto TT in engine/search.cpp

- [ ] In `handleSearchPrelude()` o metodo simile:
  - [ ] **RIMUOVERE**: `return this->probeTTCache(hashKey, depth, bounds, score);`
  - [ ] **AGGIUNGERE**: `return this->tt.probe(hashKey, depth, bounds.alpha, bounds.beta, score);`

- [ ] In `searchPosition()` o metodo che salva TT:
  - [ ] **RIMUOVERE**:
    ```cpp
    TTSaveInfo ttInfo{hashKey, depth, best, alphaOrig, bounds.beta, 0};
    this->saveTTEntry(ttInfo);
    ```
  - [ ] **AGGIUNGERE**:
    ```cpp
    const auto flag = tt::determineFlag(best, alphaOrig, bounds.beta);
    this->tt.store(hashKey, depth, best, flag);
    ```

- [ ] Grep verification (cercare usi vecchi):
  ```bash
  grep -n "probeTTCache\|saveTTEntry\|TTSaveInfo" engine/search.cpp
  # Dovrebbe essere vuoto
  ```

#### Step 3d: Eliminare tt_helpers.cpp
- [ ] Eliminare file: `rm engine/tt_helpers.cpp`
- [ ] Aggiornare `Makefile`: rimuovere `engine/tt_helpers.cpp` dalla lista sorgenti
- [ ] Verificare no include residui:
  ```bash
  grep -r "tt_helpers" engine/ --include="*.cpp" --include="*.hpp"
  # Dovrebbe essere vuoto
  ```

### Aggiornamento engine/engine.hpp
- [ ] Rimuovere: `#include "tt.hpp"`
- [ ] Aggiungere: `#include "../tt/transposition_table.hpp"`
- [ ] Rimuovere campo: `TTEntry* ttTable;` (se presente)
- [ ] Rimuovere metodi: `probeTTCache()` e `saveTTEntry()` declarations
- [ ] Verificare: nessun altro uso di tipi TT vecchi

### Aggiornamento engine/engine.cpp
- [ ] Costruttore: Rimuovere `this->ttTable = engine::globalTT();` (se presente)
- [ ] Sostituire tutte le occorrenze:
  - [ ] `engine::computeHashKey(board)` → `zobrist::computeHashKey(board)`
  - [ ] `prefetchTT(key)` → `tt::TranspositionTable::instance().prefetch(key)`
  - [ ] `incrementTTGeneration()` → `tt::TranspositionTable::instance().incrementGeneration()`
- [ ] Grep verification:
  ```bash
  grep -n "TTEntry\|globalTT\|engine::computeHashKey" engine/engine.cpp
  ```

### Build System
- [ ] Aggiornare `Makefile`:
  - [ ] Verificare che `engine/tt_helpers.cpp` NON sia più nella lista sorgenti (eliminato)
  - [ ] Verificare che nessun altro file .cpp relativo a TT sia listato

### Pulizia
- [ ] Rimuovere file vecchi (mantenere backup):
  - [ ] `rm engine/tt.hpp`
  - [ ] `rm engine/tt_helpers.cpp` (già fatto in Step 3d)
- [ ] Verificare nessun file include vecchi header TT:
  ```bash
  grep -r "#include.*tt\.hpp" --include="*.cpp" --include="*.hpp"
  # Dovrebbe essere vuoto
  ```
- [ ] Verificare nessun uso di tt_helpers:
  ```bash
  grep -r "probeTTCache\|saveTTEntry" engine/ --include="*.cpp" --include="*.hpp"
  # Dovrebbe essere vuoto (metodi eliminati)
  ```

### Testing Compilation
- [ ] Clean build: `make cls`
- [ ] Build prod: `make prod` (verificare 0 errori, 0 warning)
- [ ] Build test: `make test` (verificare 0 errori)

### Testing Funzionalità
- [ ] Run tests: `./tests/outputTest` (tutti i test devono passare)
- [ ] Run chess: `./chess` (verificare avvio corretto)
- [ ] Play some moves: verificare engine risponde correttamente

### Verifiche Finali
- [ ] `sizeof(tt::TranspositionTable::Entry)` == 16 bytes (validato da static_assert)
- [ ] `alignof(tt::TranspositionTable::Entry)` == 8 bytes (validato da static_assert)
- [ ] Nessun residuo vecchio codice:
  ```bash
  grep -r "TTEntry\|globalTT\|engine::detail" engine/ --include="*.cpp" --include="*.hpp"
  ```
- [ ] Verifica noexcept specification:
  ```bash
  grep "probe\|store\|prefetch" tt/transposition_table.hpp | grep "noexcept"
  ```

### Documentazione
- [ ] Aggiornare `CLAUDE.md`:
  - [ ] Sezione Transposition Table (sostituire descrizione vecchia)
  - [ ] Namespace: `zobrist::` e `tt::`
  - [ ] Singleton pattern
  - [ ] Esempi di utilizzo aggiornati

### Step 9: Performance Validation
- [ ] Compilare con ottimizzazioni: `make prod`
- [ ] Verificare assembly generato (opzionale):
  ```bash
  objdump -d -C chess | grep "TranspositionTable::probe" -A 50
  ```
- [ ] Test funzionalità base:
  ```bash
  ./chess
  # Giocare alcune mosse, verificare engine funziona
  ```
- [ ] Confronto informale performance (opzionale ma raccomandato):
  - [ ] Test posizione fissa: contare nodi/sec vs versione precedente
  - [ ] Verificare latency simile o migliore

### Commit
- [ ] Commit con messaggio descrittivo:
  ```
  Refactor TT: struct → class OOP + separa Zobrist + elimina bridge

  - Crea tt/zobrist.hpp (~140 righe) con std::array
  - Crea tt/transposition_table.hpp (~220 righe) con classe TT
  - Namespace globali: zobrist:: e tt:: (non engine::)
  - TranspositionTable: istanza membro in Engine (no singleton)

  - Entry con commenti layout memory espliciti
  - static_assert per validazione compile-time (16 bytes, alignment)
  - Tutti metodi critici con noexcept per ottimizzazioni
  - Prefetch con commenti dettagliati su behavior

  - Overload probe/store per int64_t (conversioni gestite da TT)
  - Helper determineFlag() riduce duplicazione in Engine
  - Move semantics abilitato (default move ctor/assignment)

  - Elimina tt_helpers.cpp completamente (file bridge inutile)
  - Engine usa this->tt.probe/store direttamente
  - Breaking change: aggiornati engine.cpp/hpp/search.cpp

  - Codice più pulito: 2 file separati (zobrist + TT)
  - Separazione responsabilità netta, testabilità migliorata
  - std::array, noexcept, static_assert: Modern C++23
  ```

---

## 8. Decisioni di Design Finalizzate

Tutte le domande aperte sono state risolte. Ecco il riepilogo delle decisioni:

1. **Directory structure**: ✅ `tt/` al livello root
   - Simmetrico con `board/`, `piece/`, `coords/`
   - Namespace globali `zobrist::` e `tt::`

2. **Backward compatibility**: ✅ NO - Breaking change diretto
   - Aggiornamento immediato di tutto il codice
   - No wrapper deprecati
   - Clean cut per maintenance più semplice

3. **Testing**: Da verificare durante implementazione
   - Se esistono test, vanno aggiornati per usare istanza membro `engine.tt`
   - Se non esistono, opportunità di creare unit test
   - Testabilità migliorata: possibile creare multiple istanze TT per test paralleli

4. **Zobrist independence**: ✅ File separato con namespace dedicato
   - `tt/zobrist.hpp` separato da `tt/transposition_table.hpp`
   - Logicamente e fisicamente separati
   - Riusabile indipendentemente, testabile in isolamento

5. **TranspositionTable features**: ✅ Minimal per ora
   - `clear()`, `getMemoryUsage()` già presenti
   - Funzionalità diagnostiche (hit rate, collisions) possono essere aggiunte dopo

6. **Entry visibility**: ✅ Public nested type
   - Accessibile come `tt::TranspositionTable::Entry`
   - Necessario per Engine helpers (flag access)
   - Mantiene encapsulation ma permette uso quando necessario

7. **Metodi inline**: ✅ Tutto inline nell'header
   - `prefetch()`, `probe()`, `store()` implementati inline
   - Zero overhead garantito
   - Header ~320 righe (accettabile per performance-critical code)

---

## 9. Vantaggi Attesi

### Encapsulation e OOP Design
- **Classe istanziabile**: Engine possiede TT, stato e comportamento unificati
- **Entry nested**: Scope limitato, `TranspositionTable::Entry` è self-documenting
- **Dati privati**: `table_` (std::array) e `generation_` non accessibili direttamente dall'esterno
- **Metodi pubblici ben definiti**: `probe()`, `store()`, `prefetch()`, `incrementGeneration()`
- **Type-safe**: `std::array` invece di C-style arrays per maggiore sicurezza e leggibilità
- **noexcept specification**: Documenta no-throw guarantee, abilita ottimizzazioni compiler

### Manutenibilità
- **File separati per responsabilità**: 2 file header (zobrist.hpp ~140 righe + transposition_table.hpp ~220 righe)
- **Responsabilità chiare**: Zobrist (hashing) separato da TT (caching)
- **Namespace chiari**: `zobrist::` e `tt::` a livello globale, struttura logica evidente
- **Nessun file bridge**: Eliminato tt_helpers.cpp, Engine usa TT direttamente
- **Modern C++23**: `std::array`, `noexcept`, `static_assert`, `constexpr`
- **Helper functions**: `xorPiecesFromBitboard()` e `determineFlag()` riducono duplicazione
- **Range-based loops**: `clear()` usa range-based for su std::array (più leggibile)
- **Commenti espliciti**: Layout memory documentato, prefetch behavior spiegato

### Testabilità
- **Interfaccia ben definita**: Facile creare mock o stub per testing
- **Zobrist indipendente**: Testabile completamente in isolamento
- **Clear API**: Metodi pubblici sono il contract per testing

### Riusabilità
- **Zobrist hashing**: Completamente separato e riutilizzabile per altre strutture hash
- **TranspositionTable**: Potenziale per future varianti (es. analysis mode)
- **Clear boundaries**: Separazione engine/TT ben definita

### Type Safety
- **Scoped enums**: `TranspositionTable::Entry::Flag::EXACT` invece di `TTEntry::EXACT` globale
- **Member functions**: `instance().probe()` invece di `probeTT(table, ...)` riduce errori
- **Deleted copy**: Previene accidentale copia del singleton

---

## 10. Rischi e Mitigazioni

| Rischio | Probabilità | Impatto | Mitigazione |
|---------|-------------|---------|-------------|
| Breaking existing code | Media | Alto | Breaking change diretto, aggiornamento immediato |
| std::array initialization overhead | Molto bassa | Basso | Default constructor, entries sovrascritte da clear() o store() |
| Include cycles | Bassa | Medio | Dependency graph semplice, TT e Zobrist indipendenti |
| Errori durante migrazione | Media | Alto | Compilation step-by-step, test dopo ogni modifica |
| Entry layout change | Molto bassa | Alto | static_assert verifica sizeof/alignof a compile-time |

---

## Note Finali

Questo refactoring rappresenta una **ristrutturazione significativa verso OOP design moderno** con i seguenti obiettivi:

### Obiettivi Primari
1. ✅ **Encapsulation**: Trasformare struct + funzioni globali in classe OOP istanziabile
2. ✅ **Modularità**: Separare Zobrist da TT per indipendenza logica (file separati + namespace separati)
3. ✅ **Manutenibilità**: 2 file ben separati invece di 1 monolitico, responsabilità chiare
4. ✅ **Eliminazione file bridge**: TT espone API con overload, elimina tt_helpers.cpp completamente
5. ✅ **Separazione netta**: Engine usa TT direttamente, conversioni nascoste in TT (API pubblica pulita)
6. ✅ **int32_t per robustezza**: Range ±2 miliardi, zero rischio overflow, Entry rimane 16 bytes

### Design Rationale

**Perché costruttore pubblico invece di singleton?**
- ✅ **Engine owns TT**: Istanza come membro, lifetime gestito da Engine
- ✅ **Accesso diretto**: `this->tt.probe()` invece di `TranspositionTable::instance().probe()`
- ✅ **Più conciso**: Elimina ripetizione di `.instance()`
- ✅ **Testabilità**: Possibile creare multiple istanze per test paralleli
- ✅ **Flessibilità**: Engine può avere più TT se necessario (es. main + analysis)
- ✅ **Zero overhead**: Nessun singleton management, solo accesso membro diretto

**Perché Entry nested invece di separata?**
- ✅ Scope limitato: Entry ha senso solo nel contesto di TranspositionTable
- ✅ Naming chiaro: `tt::TranspositionTable::Entry` è self-documenting
- ✅ Public access per Engine helpers (flag usage)
- ✅ Mantiene encapsulation pur essendo accessibile

**Perché 2 file separati invece di 1 monolitico?**
- ✅ **Separazione responsabilità**: Zobrist (hashing) e TT (cache) sono concetti distinti
- ✅ **Riusabilità**: Zobrist può essere usato senza TT (es. per altre strutture hash)
- ✅ **Testabilità**: Zobrist testabile in isolamento completo
- ✅ **Leggibilità**: File più corti (~140 + ~210 righe) invece di ~350 righe monolitiche
- ✅ **Dependency esplicita**: `transposition_table.hpp` include `zobrist.hpp` (chiaro)
- ✅ **Modularità**: Engine può includere solo zobrist.hpp se necessario
- ✅ Zero overhead: compile-time Zobrist + inline TT methods + std::array

**Perché namespace globali (`zobrist::`, `tt::`) invece di `engine::`?**
- ✅ Più puliti e concisi: `zobrist::computeHashKey()` vs `engine::zobrist::computeHashKey()`
- ✅ Riutilizzabili: concetti generali non legati specificamente all'engine
- ✅ Simmetria con altri moduli: `chess::Board`, `pieces::Piece`, `zobrist::`, `tt::`

**Perché breaking change invece di backward compatibility?**
- ✅ Clean cut: no wrapper deprecati da mantenere
- ✅ Codebase piccolo: aggiornamenti veloci e controllabili
- ✅ Migliore design: namespace e OOP senza compromessi legacy

**Perché overload int64_t invece di convertire Engine a int32_t?**
- ✅ **Engine invariato**: Nessun refactor massiccio di engine.cpp/search.cpp
- ✅ **Separazione responsabilità**: TT gestisce conversioni (dettaglio implementativo)
- ✅ **API pulita**: Engine non sa che TT usa int32_t internamente
- ✅ **Type safety**: Overload risolto a compile-time, zero overhead
- ✅ **Flessibilità futura**: Se serve int32_t, basta aggiungere overload in TT
- ✅ **Testing ridotto**: Solo TT cambia, Engine invariato (meno rischio)

### Struttura Finale

```
Repository root:
├── tt/
│   └── transposition_table.hpp  # ~350 righe (zobrist + TranspositionTable con std::array)
├── engine/
│   ├── engine.hpp               # include "../tt/transposition_table.hpp", score types: int32_t
│   ├── engine.cpp               # usa zobrist::computeHashKey(), tt::TranspositionTable::instance()
│   ├── search.cpp               # usa TT direttamente, no conversioni
│   └── (tt_helpers.cpp ELIMINATO)
└── board/, piece/, coords/, ...
```

```
Repository root:
├── tt/
│   ├── zobrist.hpp              # ~140 righe (Zobrist hashing con std::array)
│   └── transposition_table.hpp  # ~210 righe (TranspositionTable class, include zobrist.hpp)
├── engine/
│   ├── engine.hpp               # Include tt/transposition_table.hpp
│   ├── engine.cpp               # usa zobrist::computeHashKey(), tt::TranspositionTable::instance()
│   ├── search.cpp               # usa TT direttamente, no conversioni
│   └── (tt_helpers.cpp ELIMINATO)
└── board/, piece/, coords/, ...
```

**File eliminati**:
- ❌ `engine/tt.hpp` → sostituito da `tt/zobrist.hpp` + `tt/transposition_table.hpp`
- ❌ `engine/tt_helpers.cpp` → eliminato, Engine usa TT direttamente

### Prossimi Step

1. ✅ **Decisioni di design finalizzate** (Sezione 8)
2. **Implementare** seguendo checklist dettagliata (Sezione 7)
   - Step 1: Backup e preparazione
   - Step 2a: **Creare tt/zobrist.hpp** con implementazione completa (~140 righe)
   - Step 2b: **Creare tt/transposition_table.hpp** con implementazione completa (~210 righe)
   - Step 3a: **Aggiungere overload int64_t** a TranspositionTable (conversioni gestite da TT)
   - Step 3b-3d: Aggiornare Engine per usare TT direttamente, eliminare tt_helpers.cpp
   - Step 4: Aggiornare engine.cpp per nuova API TT
   - Step 5-8: Rimuovere file vecchi, aggiornare Makefile, build, testing, cleanup
3. **Testing**: compilazione → funzionalità
   - Testing dopo overload TT (dovrebbe essere trasparente)
   - Testing finale dopo eliminazione tt_helpers.cpp
4. **Documentazione**: aggiornare CLAUDE.md
5. **Commit atomico** con messaggio descrittivo

**Quando procedere**: Il piano è completo e pronto per l'implementazione. Tutte le decisioni di design sono state prese. Si può procedere con la checklist della Sezione 7.

**VANTAGGI**:
- Con overload, Engine rimane invariato (int64_t) quindi nessun refactor massiccio necessario
- **int32_t ha range enorme** (±2 miliardi): zero rischio overflow per valutazioni scacchistiche
- Le conversioni int64_t → int32_t sono banali e sicure (clamping per robustezza)
- Conversioni identiche a prima, solo spostate dentro TT (separazione responsabilità corretta)
- **std::array** migliora type-safety e leggibilità senza overhead runtime
- Helper `xorPiecesFromBitboard()` rende il codice più modulare e testabile
- Range-based for in `clear()` più leggibile di loop con indice
