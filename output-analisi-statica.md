# Analisi Statica Completa del Codice - Chess Engine

**Data**: 2026-01-09
**Tool**: cppcheck (con flag: --enable=all)
**Files analizzati**: 41 file (.cpp + .hpp, escluso stockfish)
**Files analizzabili**: 28/41 (13 file di test non analizzabili per problemi di configurazione)

---

## Sommario Esecutivo

L'analisi statica completa ha identificato **127+ segnalazioni** distribuite nelle seguenti categorie:

| Categoria | Quantità | Severità |
|-----------|----------|----------|
| Warning (alto rischio) | 2 | ALTA |
| Performance | 2 | MEDIA |
| Style - Code Issues | 23 | MEDIA |
| Style - Unused Members | 79 | BASSA |
| Style - Unused Functions | 21 | BASSA |
| Information | 13 | INFO |

**Stato generale**: Il codice è funzionale ma presenta:
- **1 bug critico** (variabile non inizializzata)
- Molti membri di classe/struct mai utilizzati (possibile dead code)
- Problemi di type safety (costruttori non explicit)
- Shadow variables che riducono la leggibilità

---

## PARTE 1: PROBLEMI CRITICI E AD ALTA PRIORITÀ

### 1. WARNING - Variabile Membro Non Inizializzata ⚠️ CRITICO

**File**: `engine/engine.cpp:13` e `engine/engine.cpp:33`
**Severità**: CRITICA

```
warning: Member variable 'Engine::isPlayerWhite' is not initialized in the constructor.
```

**Impatto**: Bug reale che può causare comportamento indefinito. DEVE essere risolto immediatamente.

**Fix richiesto**:
```cpp
// PRIMA (BUGGY):
Engine::Engine()
    : board(), depth(1), usIsWhite(true), nodeCount(0) {
    pieces::initMagicBitboards();
}

// DOPO (CORRETTO):
Engine::Engine()
    : board(), depth(1), usIsWhite(true), isPlayerWhite(true), nodeCount(0) {
    //                                    ^^^^^^^^^^^^^^^^^^^^ AGGIUNTO
    pieces::initMagicBitboards();
}
```

---

### 2. PERFORMANCE - Parametri Passati per Valore

**Files**:
- `engine/engine.cpp:33` - `std::string fen`
- `driver/driver.cpp:277` - `std::string input`

**Fix**:
```cpp
// PRIMA:
Engine::Engine(std::string fen)
void Driver::quit(std::string input) noexcept

// DOPO:
Engine::Engine(const std::string& fen)
void Driver::quit(const std::string& input) noexcept
```

---

### 3. TYPE SAFETY - Costruttori Non Explicit

**Severità**: ALTA

| File | Linea | Costruttore |
|------|-------|-------------|
| coords/coords.hpp | 35 | `Coords(const std::string&)` |
| board/board.hpp | 102 | `Board(const std::array<uint32_t, 8>&)` |
| board/board.hpp | 113 | `Board(const std::string& fen)` |
| engine/engine.hpp | 40 | `Engine(std::string fen)` |

**Problema**: Permettono conversioni implicite pericolose.

**Fix**: Aggiungere `explicit` a tutti.

---

## PARTE 2: MEMBRI DI CLASSE/STRUCT MAI USATI

### 2.1 Engine Class (engine/engine.hpp) - 38 membri inutilizzati

**Membri privati mai usati**:

| Membro | Linea | Tipo | Motivo probabile |
|--------|-------|------|------------------|
| `isPlayerWhite` | 49 | bool | Non inizializzato + mai usato |
| `isCheckMate` | 50 | bool | Feature non implementata? |
| `depth` | 52 | uint64_t | Duplicato? (c'è anche nei parametri) |
| `ttTable` | 61 | TTEntry* | Gestione TT forse cambiata |
| `nodesSearched` | 63 | static uint64_t | Statistica non usata |
| `DEFAULTDEPTH` | 65 | static constexpr | Mai referenziato |
| `moveHistory` | 66 | static std::string | Feature non implementata |
| `MAX_THREADS` | 104 | int | OpenMP forse gestisce altrove |
| `NEG_INF` | 215 | constexpr int64_t | Forse sostituito da std::numeric_limits |
| `POS_INF` | 216 | constexpr int64_t | Idem |
| `history` | 223 | int[2][64][64] | Heuristic history non usata? |
| `PIECE_VALUES` | 225 | constexpr int64_t[8] | Forse sostituito da altre tabelle |
| `ttProbes` | 70 | static uint64_t | Statistica TT |
| `ttHits` | 71 | static uint64_t | Statistica TT |

**Struct SearchContext** (tutti i membri mai usati):
- `depth` (108)
- `alpha` (109)
- `beta` (110)
- `ply` (111)
- `activeColor` (112)

**Struct AlphaBeta** (tutti i membri mai usati):
- `alpha` (116)
- `beta` (117)

**Nota**: Questo è strano perché AlphaBeta viene usato nel codice, ma cppcheck dice che i membri non sono mai acceduti. Possibile falso positivo o accesso solo tramite aggregate initialization.

**Struct TTSaveInfo** (tutti i membri mai usati):
- `hashKey` (121)
- `depth` (122)
- `alphaOrig` (124)
- `beta` (125)
- `bestMove` (126)

**Struct AttackData** (tutti i membri mai usati):
- `allAttacks` (194)
- `pawnAttacks` (195)
- `knightAttacks` (196)
- `bishopAttacks` (197)
- `rookAttacks` (198)
- `queenAttacks` (199)
- `knightMobility` (201)
- `bishopMobility` (202)
- `rookMobility` (203)
- `queenMobility` (204)

---

### 2.2 Board::MoveState (board/board.hpp) - 15 membri inutilizzati

**Tutti i seguenti membri sono dichiarati ma mai usati**:

| Membro | Linea | Tipo |
|--------|-------|------|
| `prevActiveColor` | 70 | uint8_t |
| `prevHalfMoveClock` | 71 | uint16_t |
| `prevFullMoveClock` | 72 | uint16_t |
| `prevCastle` | 76 | uint8_t |
| `prevHasMoved` | 77 | uint8_t |
| `capturedPiece` | 80 | uint8_t |
| `fromPiece` | 81 | uint8_t |
| `promotionPieceType` | 82 | uint8_t |
| `wasEnPassantCapture` | 84 | bool |
| `enPassantCapturedIndex` | 85 | uint8_t |
| `wasCastling` | 87 | bool |
| `rookFromIndex` | 88 | uint8_t |
| `rookToIndex` | 89 | uint8_t |

**Analisi**: Questo è preoccupante. `MoveState` dovrebbe contenere tutte le informazioni per unmovePiece, ma se i membri non sono mai acceduti, possibile che:
1. Il sistema di undo non funzioni correttamente
2. O cppcheck non riesce a tracciare l'accesso aggregato

---

### 2.3 Driver (driver/driver.hpp) - 5 membri inutilizzati

| Membro | Linea | Descrizione |
|--------|-------|-------------|
| `MAX_PARAM_LENGTH` | 13 | constexpr static int32_t |
| `MODE` | 14 | constexpr static int32_t |
| `COLOR` | 15 | constexpr static int32_t |
| `NO_ARGS` | 16 | constexpr static int32_t |
| `menu` | 18 | print::Menu |

---

### 2.4 TTEntry (engine/tt.hpp) - 2 membri inutilizzati

| Membro | Linea | Descrizione |
|--------|-------|-------------|
| `padding` | 20 | uint8_t[3] - padding per allineamento |
| `ADJUSTMENT` | 34 | static constexpr int32_t |

---

## PARTE 3: PROBLEMI DI STYLE E QUALITÀ

### 3.1 Shadow Variables (engine/search.cpp)

**10+ occorrenze** di variabili che oscurano altre variabili:

```cpp
// Variabile esterna:
const uint8_t from = poplsb(const_cast<uint64_t&>(kings));
uint64_t mask = pieces::KING_ATTACKS[from] & ~ownOcc;

// Nei loop successivi, ridichiara le stesse variabili (SHADOW!):
while (bb) {
    const uint8_t from = poplsb(bb);  // linee 528, 535, 542, 549, 556
    uint64_t mask = ...;               // linee 529, 536, 543, 550, 557
}
```

**Altre shadow variables**:
- `eval` in `evaluate.cpp:794` oscura `engine.hpp:54`
- `depth` in `search.cpp:714` oscura `engine.hpp:52`
- `inCheck` in `board.cpp:12` oscura la funzione `inCheck()`

---

### 3.2 Condizioni Logiche Problematiche

**Condizione duplicata** (`board/boardenginemove.cpp:173 e 182`):
```cpp
if (movingType == PAWN) {     // linea 173
    // codice A
}
// ...
if (movingType == PAWN) {     // linea 182 - DUPLICATO!
    // codice B
}
```

**Condizione sempre falsa** (`board/fen.cpp:75`):
```cpp
if (enPassantSection.size() != 2 || enPassantSection == "-") {
    //                                 ^^^^^^^^^^^^^^^^^^^
    //                                 sempre false perché "-" ha size 1
}
```

---

### 3.3 Variabili che Possono Essere Const

**File**: `piece/piece.hpp`
```cpp
// PRIMA:
for (auto &offset : KNIGHT_OFFSET) {    // linea 336
for (auto &offset : KING_OFFSET) {      // linea 351

// DOPO:
for (const auto &offset : KNIGHT_OFFSET) {
for (const auto &offset : KING_OFFSET) {
```

**Parametri che dovrebbero essere const reference** (`engine/search.cpp`):
- altro metodo - parametro `ctx` (linea 71)
- `undoAndUpdateMove()` - parametro `state` (linea 293)

---

### 3.4 Variabili Non Usate

- `engine/tt.hpp:48` - `static TTGlobal data` non assegnato
- `board/boardenginemove.cpp:227` - `pieceType = PAWN` assegnato ma mai letto
- `engine/engine.cpp:143` - `km2` ha scope riducibile

---

## PARTE 4: FUNZIONI MAI USATE (DEAD CODE)

**Totale**: 21 funzioni

### Board Module (6 funzioni)
| Funzione | File:Linea | Note |
|----------|------------|------|
| `getHasMoved()` | board.hpp:158 | API pubblica? |
| `getHalfMoveClock()` | board.hpp:181 | API pubblica? |
| `setNextTurn()` | board.hpp:208 | API pubblica? |
| `setPrevTurn()` | board.hpp:217 | API pubblica? |
| `CHESSBOARD_SIZE()` | board.hpp:241 | Utilità debug? |
| `parseCastling()` | fen.cpp:57 | Legacy? |

### Engine Module (11 funzioni)
| Funzione | File:Linea | Note |
|----------|------------|------|
| `shouldPruneLateMove()` | engine.cpp:92 | Ottimizzazione sperimentale? |
| `adjacentFilesMask()` | evaluate.cpp:60 | Utilità non usata |
| `back()` | movelist.hpp:39 | API MoveList |
| `front()` | movelist.hpp:42 | API MoveList |
| `clear()` | movelist.hpp:56 | API MoveList |
| `mirrorIndex()` | piecevaluetables.hpp:103 | Utilità PST |
| `addPromotionBonus()` | search.cpp:585 | Bonus sperimentale? |
| `addCheckBonus()` | search.cpp:594 | Bonus sperimentale? |
| `addKillerAndHistoryBonus()` | search.cpp:604 | Heuristic non usata |
| `addKingMoveBonus()` | search.cpp:624 | Bonus sperimentale? |
| `incrementTTGeneration()` | tt.hpp:56 | Gestione TT |

### Printer Module (3 funzioni)
| Funzione | File:Linea | Note |
|----------|------------|------|
| `playWithPlayerMenu()` | menu.cpp:56 | Menu non implementato |
| `getPrintableBoard()` | prints.cpp:7 | Debug? |
| `getBitBoard()` | prints.cpp:42 | Debug bitboard |

### Altri (2 funzioni)
| Funzione | File:Linea | Note |
|----------|------------|------|
| `Coords::update()` | coords.hpp:91 | Utilità |
| `ExternalEngine::valid()` | driver.cpp:361 | Nested class |

---

## PARTE 5: FILE DI TEST NON ANALIZZABILI

**13 file di test** non sono stati analizzati per problemi di configurazione:

### Test Files con Problemi
```
tests/mainPerformanceTest.cpp
tests/ut.hpp
tests/mainTest.cpp
piece/test/testRook.cpp
piece/test/testKnight.cpp
piece/test/testHelper.cpp
piece/test/testPawn.cpp
piece/test/testKing.cpp
engine/test/performance-test/performanceEngine.cpp
engine/test/criticalPositionEngine.cpp
engine/test/matePosition.cpp
engine/test/EndingGame.cpp
engine/test/testEngine.cpp
```

**Errore comune**:
```
error: #error "[Boost::ext].UT requires support for rvalue references"
information: Too many #ifdef configurations - cppcheck only checks 12 of 24 configurations
```

**Nota**: I test usano la libreria Boost.UT che richiede configurazioni specifiche. I file di test potrebbero avere problemi propri ma non sono analizzabili con questa configurazione.

---

## PARTE 6: STATISTICHE DETTAGLIATE

### Per Categoria

| Categoria | Dettaglio | Quantità |
|-----------|-----------|----------|
| **Warnings** | Variabile non inizializzata | 2 |
| **Performance** | Pass-by-value string | 2 |
| **Style - Type Safety** | Costruttori non explicit | 4 |
| **Style - Shadow** | Shadow variables | 11 |
| **Style - Const** | Variabili che possono essere const | 7 |
| **Style - Logic** | Condizioni problematiche | 2 |
| **Style - Scope** | Scope riducibile | 1 |
| **Unused Members - Engine** | Membri classe Engine | 38 |
| **Unused Members - MoveState** | Membri struct MoveState | 15 |
| **Unused Members - Altri** | Driver, TTEntry, etc. | 12 |
| **Unused Functions** | Funzioni mai chiamate | 21 |
| **Information** | File test non analizzabili | 13 |

### Per Modulo (file analizzabili)

| Modulo | Warning | Perf | Style | Unused Members | Unused Func |
|--------|---------|------|-------|----------------|-------------|
| **engine/** | 2 | 1 | 15 | 38 | 11 |
| **board/** | 0 | 0 | 4 | 15 | 6 |
| **piece/** | 0 | 0 | 2 | 0 | 0 |
| **coords/** | 0 | 0 | 1 | 0 | 1 |
| **driver/** | 0 | 1 | 0 | 5 | 1 |
| **printer/** | 0 | 0 | 0 | 0 | 3 |

---

## PARTE 7: COMMENTI E PROPOSTE DI MIGLIORAMENTO

### PRIORITÀ 1 - CRITICA (Fare SUBITO)

#### Fix: Variabile Non Inizializzata

**File da modificare**: `engine/engine.cpp`

```cpp
// Costruttore 1 (linea 13):
Engine::Engine()
    : board(), depth(1), usIsWhite(true), isPlayerWhite(true), nodeCount(0) {
    //                                    ^^^^^^^^^^^^^^^^^^^^ AGGIUNTO
    pieces::initMagicBitboards();
}

// Costruttore 2 (linea 33):
Engine::Engine(const std::string& fen)  // NOTA: anche cambiato a const&
    : board(fen), depth(1), usIsWhite(true), isPlayerWhite(true), nodeCount(0) {
    //                                        ^^^^^^^^^^^^^^^^^^^^ AGGIUNTO
    pieces::initMagicBitboards();
}
```

**Tempo stimato**: 5 minuti
**Impatto**: Risolve un bug critico

---

### PRIORITÀ 2 - ALTA (Fare questa settimana)

#### 2.1 Aggiungere `explicit` ai Costruttori

**Files da modificare**:

```cpp
// coords/coords.hpp:35
explicit Coords(const std::string& input) noexcept : index(INVALID_COORDS) {

// board/board.hpp:102
explicit Board(const std::array<uint32_t, 8>& chessboard) noexcept

// board/board.hpp:113
explicit Board(const std::string& fen) {

// engine/engine.hpp:40
explicit Engine(const std::string& fen);  // NOTA: cambiato a const& anche
```

**Tempo stimato**: 30 minuti (+ testing)

---

#### 2.2 Fix Pass-by-Value

```cpp
// engine/engine.cpp:33 + engine.hpp
Engine::Engine(const std::string& fen)

// driver/driver.cpp:277 + driver.hpp
void Driver::quit(const std::string& input) noexcept
```

**Tempo stimato**: 10 minuti

---

### PRIORITÀ 3 - MEDIA (Fare questo mese)

#### 3.1 Investigare Membri Inutilizzati

**Azione richiesta**: Analizzare se i membri "unused" sono davvero inutilizzati o se sono falsi positivi.

**Focus principale**: `Board::MoveState`

Questo è il più preoccupante. Il sistema move/unmove si basa su MoveState, ma cppcheck dice che i membri non sono mai usati. Possibile che:
1. Vengano acceduti solo tramite memcpy/aggregate
2. Ci sia davvero dead code
3. Falso positivo di cppcheck

**Azione suggerita**:
```cpp
// Aggiungere attributo per confermare l'uso:
struct MoveState {
    [[maybe_unused]] uint8_t prevActiveColor{};  // Se davvero serve ma cppcheck non lo vede
    // OPPURE rimuovere se davvero non serve
};
```

**Tempo stimato**: 2-3 ore per analisi + cleanup

---

#### 3.2 Fix Shadow Variables

**File principale**: `engine/search.cpp:generateLegalMoves()`

**Strategia**: Rinominare le variabili nei loop interni:

```cpp
// PRIMA:
const uint8_t from = poplsb(const_cast<uint64_t&>(kings));
uint64_t mask = pieces::KING_ATTACKS[from] & ~ownOcc;

while (pawnBB) {
    const uint8_t from = poplsb(pawnBB);  // SHADOW!
    uint64_t mask = ...;                   // SHADOW!
}

// DOPO:
const uint8_t kingFrom = poplsb(const_cast<uint64_t&>(kings));
uint64_t kingMask = pieces::KING_ATTACKS[kingFrom] & ~ownOcc;

while (pawnBB) {
    const uint8_t pawnFrom = poplsb(pawnBB);  // chiaro
    uint64_t pawnMask = ...;                   // chiaro
}
```

**Tempo stimato**: 1-2 ore

---

#### 3.3 Fix Condizioni Logiche

**File 1**: `board/boardenginemove.cpp:173-182`
- Analizzare se la condizione duplicata è un bug o logica intenzionale
- Unire i due blocchi se fanno la stessa cosa

**File 2**: `board/fen.cpp:75`
```cpp
// PRIMA (logica errata):
if (enPassantSection.size() != 2 || enPassantSection == "-") {

// DOPO (logica corretta):
if (enPassantSection == "-" || enPassantSection.size() != 2) {
// O meglio ancora:
if (enPassantSection == "-") {
    // nessun en passant
} else if (enPassantSection.size() != 2) {
    throw std::runtime_error("Invalid en passant format");
}
```

**Tempo stimato**: 30 minuti

---

### PRIORITÀ 4 - BASSA (Opzionale)

#### 4.1 Aggiungere const dove manca

```cpp
// Range-for loops:
for (const auto &offset : KNIGHT_OFFSET) {  // piece.hpp:336
for (const auto &offset : KING_OFFSET) {    // piece.hpp:351
```

**Tempo stimato**: 1 ora

---

#### 4.2 Cleanup Dead Code

**Approccio conservativo**:

1. **Mantenere** (API pubblica):
   - `Board::getHasMoved()`, `getHalfMoveClock()`, `setNextTurn()`, `setPrevTurn()`
   - `MoveList::back()`, `front()`, `clear()`

2. **Marcare come debug** (con #ifdef DEBUG):
   - `Prints::getPrintableBoard()`
   - `Prints::getBitBoard()`

3. **Rimuovere** (chiaramente inutilizzato):
   - `ExternalEngine::valid()`
   - `Board::CHESSBOARD_SIZE()` (se mai usato)

4. **Investigare** (potrebbero essere sperimentali):
   - `Engine::shouldPruneLateMove()`
   - `Engine::addPromotionBonus()` e simili
   - Controllare git history per capire se sono stati disabilitati

**Tempo stimato**: 2-3 ore

---

## PARTE 8: RACCOMANDAZIONI GENERALI

### 8.1 Miglioramenti C++23

#### Usare `[[nodiscard]]` dove appropriato:
```cpp
[[nodiscard]] bool isValid() const;
[[nodiscard]] std::optional<Move> findBestMove();
```

#### Usare `[[maybe_unused]]` per membri intenzionalmente non usati:
```cpp
struct MoveState {
    [[maybe_unused]] uint8_t padding[3];  // per allineamento
};
```

#### Usare `std::string_view` invece di `const std::string&`:
```cpp
// Invece di:
void process(const std::string& fen);

// Usare:
void process(std::string_view fen);  // no allocazione, no copia
```

---

### 8.2 Gestione dei Test

I file di test non sono analizzabili. Opzioni:
1. Aggiungere configurazione specifica per Boost.UT
2. Escluderli dall'analisi statica (sono test, meno critici)
3. Usare un framework di test più standard (Google Test, Catch2)

---

### 8.3 Documentazione

Considerare di documentare:
1. Perché certi membri sono "unused" (se intenzionale)
2. Quali funzioni sono API pubblica vs implementazione
3. Quali funzioni sono sperimentali/WIP

Esempio:
```cpp
class Engine {
    // Public API
    Move getBestMove();

    // Experimental (not yet used)
    bool shouldPruneLateMove(...);

    // Debug only
    #ifdef DEBUG_MODE
    void printDebugInfo();
    #endif
};
```

---

## PARTE 9: PIANO DI AZIONE DETTAGLIATO

### Sprint 1 - Critici (1 settimana)
| Task | File | Tempo | Priorità |
|------|------|-------|----------|
| Fix variabile non inizializzata | engine/engine.cpp | 10 min | P0 |
| Test post-fix | tutti | 30 min | P0 |
| Aggiungere explicit | 4 file header | 30 min | P1 |
| Fix pass-by-value | engine.cpp, driver.cpp | 15 min | P1 |
| Test regressione | tutti | 1 ora | P1 |
| **TOTALE SPRINT 1** | | **~2.5 ore** | |

### Sprint 2 - Qualità Codice (2 settimane)
| Task | File | Tempo | Priorità |
|------|------|-------|----------|
| Fix shadow variables | engine/search.cpp | 2 ore | P2 |
| Fix condizioni logiche | board/*.cpp | 1 ora | P2 |
| Aggiungere const | vari | 1 ora | P3 |
| Investigare MoveState unused | board/board.hpp | 2 ore | P2 |
| **TOTALE SPRINT 2** | | **~6 ore** | |

### Sprint 3 - Cleanup (tempo libero)
| Task | File | Tempo | Priorità |
|------|------|-------|----------|
| Analisi dead functions | vari | 3 ore | P4 |
| Cleanup membri unused | engine.hpp | 2 ore | P3 |
| Documentazione decisioni | docs | 1 ora | P4 |
| **TOTALE SPRINT 3** | | **~6 ore** | |

**TOTALE GENERALE**: ~15 ore per un codebase molto più pulito e sicuro

---

## PARTE 10: CONCLUSIONI

### Punti di Forza
- Architettura solida (bitboards, magic numbers)
- Uso corretto di C++23
- Pochi bug reali (solo 1 critico trovato)
- Buona separazione dei moduli

### Punti di Debolezza
- **1 bug critico** (variabile non inizializzata) - DA RISOLVERE SUBITO
- Molti membri "unused" (possibile over-engineering o falsi positivi)
- Manca type safety (costruttori non explicit)
- Shadow variables riducono leggibilità
- Codice morto non rimosso

### Raccomandazione Finale

**Azione immediata** (oggi):
1. Fix bug critico `isPlayerWhite` (10 minuti)
2. Test che il programma funzioni correttamente
3. Commit: "fix: initialize isPlayerWhite in Engine constructors"

**Azione breve termine** (questa settimana):
1. Aggiungere `explicit` ai costruttori (30 min)
2. Fix pass-by-value (15 min)
3. Test completo
4. Commit: "refactor: improve type safety and performance"

**Azione medio termine** (prossime 2 settimane):
1. Fix shadow variables
2. Investigare membri unused
3. Cleanup condizioni logiche

**ROI (Return on Investment)**:
- **Critico** (Sprint 1): 2.5 ore → elimina 1 bug critico + migliora type safety → **ROI ALTISSIMO**
- **Medio** (Sprint 2): 6 ore → migliora leggibilità e manutenibilità → **ROI ALTO**
- **Basso** (Sprint 3): 6 ore → cleanup estetico → **ROI MEDIO-BASSO**

---

## Note Finali

**Files non analizzati**: 13 file di test (problema di configurazione Boost.UT)
**Files analizzati con successo**: 28/41 (68%)
**Checker attivi**: 161/592 (27% - configurazione standard)

Per un'analisi ancora più approfondita, si potrebbe:
1. Usare `--force` per analizzare tutte le configurazioni #ifdef
2. Configurare correttamente Boost.UT per analizzare i test
3. Usare altri tool (clang-tidy, PVS-Studio, etc.) per confronto
