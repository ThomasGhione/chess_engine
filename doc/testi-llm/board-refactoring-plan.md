# Board Class Refactoring Plan

## Obiettivo
Ristrutturare `Board` per migliorare leggibilità e prestazioni attraverso decomposizione modulare e ottimizzazioni mirate.

---

## Analisi Criticità Attuali

### Problemi di Leggibilità
- **Monoliticità**: classe da 535 righe (hpp) + 798 righe (cpp)
- **Responsabilità multiple**: rappresentazione, validazione, move generation, FEN parsing
- **Helper privati**: 12 metodi di validazione mescolati con stato
- **Bitboard pubblici**: esposizione diretta (`pawns_bb`, `rooks_bb`, ecc.)

### Problemi di Prestazioni
- **Overhead getter/setter**: conversioni coordinate ripetute (7-rank → internal row)
- **Aggiornamento bitboard**: chiamate a `addPieceToBB`/`removePieceFromBB` in ogni move
- **Branch misprediction**: switch statements su piece_id, `[[unlikely]]` non sempre efficaci
- **Cache pollution**: struct MoveState grande (96 byte), frequenti accessi sparsi

---

## Refactoring Proposto

### 1. Estrazione Moduli

#### 1.1 Bitboard Manager (`board/bitboard_manager.hpp`)
**Responsabilità**: Gestione centralizzata dei bitboard piece-specific e occupancy

```cpp
struct BitboardManager {
    uint64_t occupancy;
    std::array<uint64_t, 2> pawns, knights, bishops, rooks, queens, kings;
    
    void update(uint8_t index, uint8_t piece);
    void fullRebuild(const board& chessboard);
    void fastMove(uint8_t from, uint8_t to, uint8_t piece);
};
```

**Benefici**:
- Riduce dimensione Board (192 byte → ~100 byte)
- Centralizza logica bitboard (elimina 3 metodi dalla classe principale)
- Migliora locality: accessi bitboard raggruppati

---

#### 1.2 Move Validator (`board/move_validator.hpp`)
**Responsabilità**: Tutta la logica di validazione move (canMoveToBB + helper)

```cpp
class MoveValidator {
    bool canMoveToBB(const Board&, Coords, Coords, bool inCheck);
    // 12 metodi privati di Board diventano metodi statici qui
};
```

**Benefici**:
- Separa validazione da rappresentazione
- Testabilità isolata
- Riduce complessità ciclomatica di Board

---

#### 1.3 FEN Parser (`board/fen_parser.hpp`)
**Responsabilità**: Parsing/serializzazione FEN (già parziale in `fen.cpp`)

**Azioni**:
- Consolidare `fromFenToBoard`/`fromBoardToFen` + helper
- Rimuovere 8 metodi privati da Board

---

### 2. Ottimizzazioni Prestazioni

#### 2.1 Index-Only API
**Problema**: Conversione Coords ↔ index ripetuta in loop critici

**Soluzione**:
```cpp
// PRIMA:
uint8_t get(Coords c) const { return get(c.index); } // extra call

// DOPO (inline forzato):
__attribute__((always_inline))
uint8_t get(Coords c) const { 
    const uint8_t rank = c.index >> 3;
    const uint8_t row = 7 - rank;
    return (chessboard[row] >> ((c.index & 7) << 2)) & MASK_PIECE;
}
```

**Benefici**:
- Elimina overhead di chiamata
- Migliora inlining optimizer

---

#### 2.2 Compact MoveState
**Problema**: MoveState usa 96 byte per singolo undo

**Soluzione**:
```cpp
struct MoveState {
    // Layout packed: 64 bit
    // [0]      prevActiveColor (1 bit)
    // [1-16]   prevHalfMoveClock (16 bit)
    // [17-32]  prevFullMoveClock (16 bit)
    // [33-40]  prevEnPassant index (8 bit)
    // [41-48]  prevCastle (8 bit)
    // [49-56]  prevHasMoved (8 bit)
    uint64_t packed;
    
    // Layout captured: 64 bit
    // [0-3]    capturedPiece (4 bit)
    // [4-7]    fromPiece (4 bit)
    // [8-11]   promotionPieceType (4 bit)
    // [12-18]  enPassantCapturedIndex (7 bit)
    // [19-25]  rookFromIndex (7 bit)
    // [26-32]  rookToIndex (7 bit)
    // [33]     wasEnPassantCapture (1 bit)
    // [34]     wasCastling (1 bit)
    uint64_t captured;
    
    // Getters inline constexpr
    [[nodiscard]] constexpr uint8_t getPrevActiveColor() const noexcept {
        return packed & 1;
    }
    [[nodiscard]] constexpr uint16_t getPrevHalfMoveClock() const noexcept {
        return (packed >> 1) & 0xFFFF;
    }
    [[nodiscard]] constexpr uint16_t getPrevFullMoveClock() const noexcept {
        return (packed >> 17) & 0xFFFF;
    }
    [[nodiscard]] constexpr uint8_t getPrevEnPassantIndex() const noexcept {
        return (packed >> 33) & 0xFF;
    }
    [[nodiscard]] constexpr uint8_t getPrevCastle() const noexcept {
        return (packed >> 41) & 0xFF;
    }
    [[nodiscard]] constexpr uint8_t getPrevHasMoved() const noexcept {
        return (packed >> 49) & 0xFF;
    }
    
    [[nodiscard]] constexpr uint8_t getCapturedPiece() const noexcept {
        return captured & 0xF;
    }
    [[nodiscard]] constexpr uint8_t getFromPiece() const noexcept {
        return (captured >> 4) & 0xF;
    }
    [[nodiscard]] constexpr uint8_t getPromotionPieceType() const noexcept {
        return (captured >> 8) & 0xF;
    }
    [[nodiscard]] constexpr uint8_t getEnPassantCapturedIndex() const noexcept {
        return (captured >> 12) & 0x7F;
    }
    [[nodiscard]] constexpr uint8_t getRookFromIndex() const noexcept {
        return (captured >> 19) & 0x7F;
    }
    [[nodiscard]] constexpr uint8_t getRookToIndex() const noexcept {
        return (captured >> 26) & 0x7F;
    }
    [[nodiscard]] constexpr bool wasEnPassantCapture() const noexcept {
        return (captured >> 33) & 1;
    }
    [[nodiscard]] constexpr bool wasCastling() const noexcept {
        return (captured >> 34) & 1;
    }
    
    // Setters inline constexpr
    constexpr void setPrevActiveColor(uint8_t val) noexcept {
        packed = (packed & ~1ULL) | (val & 1);
    }
    constexpr void setPrevHalfMoveClock(uint16_t val) noexcept {
        packed = (packed & ~(0xFFFFULL << 1)) | (static_cast<uint64_t>(val & 0xFFFF) << 1);
    }
    constexpr void setPrevFullMoveClock(uint16_t val) noexcept {
        packed = (packed & ~(0xFFFFULL << 17)) | (static_cast<uint64_t>(val & 0xFFFF) << 17);
    }
    constexpr void setPrevEnPassantIndex(uint8_t val) noexcept {
        packed = (packed & ~(0xFFULL << 33)) | (static_cast<uint64_t>(val) << 33);
    }
    constexpr void setPrevCastle(uint8_t val) noexcept {
        packed = (packed & ~(0xFFULL << 41)) | (static_cast<uint64_t>(val) << 41);
    }
    constexpr void setPrevHasMoved(uint8_t val) noexcept {
        packed = (packed & ~(0xFFULL << 49)) | (static_cast<uint64_t>(val) << 49);
    }
    
    constexpr void setCapturedPiece(uint8_t val) noexcept {
        captured = (captured & ~0xFULL) | (val & 0xF);
    }
    constexpr void setFromPiece(uint8_t val) noexcept {
        captured = (captured & ~(0xFULL << 4)) | (static_cast<uint64_t>(val & 0xF) << 4);
    }
    constexpr void setPromotionPieceType(uint8_t val) noexcept {
        captured = (captured & ~(0xFULL << 8)) | (static_cast<uint64_t>(val & 0xF) << 8);
    }
    constexpr void setEnPassantCapturedIndex(uint8_t val) noexcept {
        captured = (captured & ~(0x7FULL << 12)) | (static_cast<uint64_t>(val & 0x7F) << 12);
    }
    constexpr void setRookFromIndex(uint8_t val) noexcept {
        captured = (captured & ~(0x7FULL << 19)) | (static_cast<uint64_t>(val & 0x7F) << 19);
    }
    constexpr void setRookToIndex(uint8_t val) noexcept {
        captured = (captured & ~(0x7FULL << 26)) | (static_cast<uint64_t>(val & 0x7F) << 26);
    }
    constexpr void setWasEnPassantCapture(bool val) noexcept {
        captured = (captured & ~(1ULL << 33)) | (static_cast<uint64_t>(val) << 33);
    }
    constexpr void setWasCastling(bool val) noexcept {
        captured = (captured & ~(1ULL << 34)) | (static_cast<uint64_t>(val) << 34);
    }
};
```

**Benefici**:
- Riduce a 16 byte (6x compressione)
- Migliora cache utilization in search iterativo
- Getters/setters `constexpr` inline: zero overhead a runtime

---

#### 2.3 Piece Dispatch Table
**Problema**: Switch su piece_id causa branch misprediction

**Soluzione** - Array statico constexpr invece di function pointer:
```cpp
// Function pointer approach (scartato - overhead indirezione)
// using MoveGenFn = uint64_t(*)(uint8_t, uint64_t);
// constexpr MoveGenFn MOVE_GEN_TABLE[7] = {...};

// MEGLIO: Array statico constexpr con template dispatch
template<uint8_t PieceType>
static constexpr uint64_t generateMoves(uint8_t index, uint64_t occupancy) noexcept {
    if constexpr (PieceType == PAWN)   return pieces::getPawnMoves(index, occupancy);
    if constexpr (PieceType == KNIGHT) return pieces::KNIGHT_ATTACKS[index];
    if constexpr (PieceType == BISHOP) return pieces::getBishopAttacks(index, occupancy);
    if constexpr (PieceType == ROOK)   return pieces::getRookAttacks(index, occupancy);
    if constexpr (PieceType == QUEEN)  return pieces::getQueenAttacks(index, occupancy);
    if constexpr (PieceType == KING)   return pieces::KING_ATTACKS[index];
    return 0ULL;
}

// Dispatch runtime tramite jump table del compilatore
static constexpr auto DISPATCH = [](uint8_t type, uint8_t idx, uint64_t occ) {
    switch(type) {
        case PAWN:   return generateMoves<PAWN>(idx, occ);
        case KNIGHT: return generateMoves<KNIGHT>(idx, occ);
        case BISHOP: return generateMoves<BISHOP>(idx, occ);
        case ROOK:   return generateMoves<ROOK>(idx, occ);
        case QUEEN:  return generateMoves<QUEEN>(idx, occ);
        case KING:   return generateMoves<KING>(idx, occ);
        default:     return 0ULL;
    }
};
```

**Benefici**:
- Template dispatch: specializzazione compile-time, zero overhead
- Nessun indirection (function pointer richiede load da memoria)
- Jump table del compilatore: 1 ciclo CPU vs 3-5 con branch misprediction
- Possibilità di inlining aggressive per il compilatore
---

### 3. Decomposizione Metodi Lunghi

#### 3.1 `moveBB` (173 righe)
**Problema**: Gestisce 7 casi (move, capture, en passant, castling, promotion, clocks, castling rights)

**Soluzione**:
```cpp
bool moveBB(Coords from, Coords to) {
    MoveContext ctx = buildContext(from, to);
    if (!validate(ctx)) return false;
    
    executeMove(ctx);
    updateGameState(ctx);
    return true;
}
```

**Benefici**:
- Singolo metodo → 4 metodi da 30-40 righe
- Testabilità per fase

---

#### 3.2 `hasAnyLegalMove` (115 righe)
**Problema**: 6 loop while quasi-identici (solo Knights/Bishops/Rooks/Queens sono duplicati)

**Soluzione** - Helper per pezzi semplici (sliding + knight):
```cpp
// Helper per pezzi con pattern identico (Knight, Bishop, Rook, Queen)
template<typename MoveGenFn>
[[nodiscard]] inline bool hasLegalMovesSimplePiece(
    uint64_t pieceBB, 
    uint64_t ownOcc,
    bool inCheck,
    MoveGenFn moveGen
) const noexcept {
    while (pieceBB) {
        const uint8_t from = __builtin_ctzll(pieceBB);
        pieceBB &= pieceBB - 1;
        
        uint64_t movesMask = moveGen(from) & ~ownOcc;
        while (movesMask) {
            const uint8_t to = __builtin_ctzll(movesMask);
            movesMask &= movesMask - 1;
            if (canMoveToBB(Coords{from}, Coords{to}, inCheck)) return true;
        }
    }
    return false;
}

// hasAnyLegalMove diventa:
bool hasAnyLegalMove(uint8_t color) const noexcept {
    const int side = (color == WHITE) ? 0 : 1;
    const uint64_t ownOcc = /* ... */;
    const bool inChk = inCheck(color);
    
    // King (ha castling - mantieni manuale)
    if (hasLegalMovesKing(side, inChk)) return true;
    
    // Pawns (ha pushes + captures - mantieni manuale)
    if (hasLegalMovesPawns(side, inChk)) return true;
    
    // Pezzi semplici (DRY con template)
    if (hasLegalMovesSimplePiece(knights_bb[side], ownOcc, inChk, 
        [](uint8_t sq) { return pieces::KNIGHT_ATTACKS[sq]; })) return true;
        
    if (hasLegalMovesSimplePiece(bishops_bb[side], ownOcc, inChk,
        [this](uint8_t sq) { return pieces::getBishopAttacks(sq, occupancy); })) return true;
        
    if (hasLegalMovesSimplePiece(rooks_bb[side], ownOcc, inChk,
        [this](uint8_t sq) { return pieces::getRookAttacks(sq, occupancy); })) return true;
        
    if (hasLegalMovesSimplePiece(queens_bb[side], ownOcc, inChk,
        [this](uint8_t sq) { return pieces::getQueenAttacks(sq, occupancy); })) return true;
    
    return false;
}
```

**Benefici**:
- Elimina duplicazione per 4 pezzi (Knights/Bishops/Rooks/Queens)
- King e Pawns rimangono separati (casi speciali legittimi)
- Riduce da 115 a ~70 righe
- Migliora manutenibilità senza sacrificare prestazioni

---

### 4. Miglioramento Incapsulamento

#### 4.1 Bitboard Pubblici
**Stato attuale**: `pawns_bb`, `rooks_bb`, ecc. sono pubblici

**Decisione**: **Mantenere pubblici per prestazioni**

**Motivazione**:
- Engine accede a bitboard in loop critici (migliaia di volte/secondo)
- Qualsiasi getter/wrapper introduce overhead potenziale
- Accesso diretto = zero latenza garantita
- Board è già una struttura dati low-level (non API pubblica)

**Miglioramento leggibilità** - Raggruppa e documenta:
```cpp
class Board {
public:
    // ============================================
    // BITBOARDS - Public per prestazioni critiche
    // Accesso diretto richiesto da Engine/MoveValidator
    // [0] = WHITE, [1] = BLACK
    // ============================================
    std::array<uint64_t, 2> pawns_bb   = {0ULL, 0ULL};
    std::array<uint64_t, 2> knights_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> bishops_bb = {0ULL, 0ULL};
    std::array<uint64_t, 2> rooks_bb   = {0ULL, 0ULL};
    std::array<uint64_t, 2> queens_bb  = {0ULL, 0ULL};
    std::array<uint64_t, 2> kings_bb   = {0ULL, 0ULL};
    uint64_t occupancy = 0ULL;  // Combined occupancy di tutti i pezzi

private:
    // ============================================
    // BOARD STATE - Rappresentazione principale
    // ============================================
    board chessboard;  // Array 8x32bit (4 bit per pezzo)
    
    // ============================================
    // GAME STATE - Stato partita
    // ============================================
    uint8_t  castle = 0x0F;      // Castling rights (KQkq)
    uint8_t  hasMoved = 0x00;    // Piece movement tracking
    Coords   enPassant{};        // En passant target square
    uint16_t halfMoveClock = 0;  // 50-move rule counter
    uint16_t fullMoveClock = 1;  // Move number
    uint8_t  activeColor = WHITE;
    
    std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
};
```

**Benefici**:
- Sezioni visivamente separate con commenti strutturati
- Documentazione esplicita del perché sono pubblici
- Convenzione [0]=WHITE, [1]=BLACK chiaramente indicata
- Nessun overhead prestazionale

---

#### 4.2 Costanti Magiche
**Problema**: Indici hardcoded (`56`, `63`, `0x0F`, ecc.)

**Soluzione**:
```cpp
enum CastlingBits : uint8_t {
    WHITE_KINGSIDE  = 0,
    WHITE_QUEENSIDE = 1,
    BLACK_KINGSIDE  = 2,
    BLACK_QUEENSIDE = 3
};

static constexpr uint8_t WHITE_KING_START = 60;
static constexpr uint8_t BLACK_KING_START = 4;
```

---

## Riepilogo Interventi

| Categoria | Azione | Impatto Leggibilità | Impatto Prestazioni |
|-----------|--------|---------------------|---------------------|
| Estrazione moduli | 3 nuovi file | ✓✓✓ | ✓ |
| Index-only API | Inline forzati | ✓ | ✓✓ |
| MoveState compatto | Riduzione 6x | ✓ | ✓✓✓ |
| Dispatch table | Function pointers | ✓✓ | ✓✓ |
| Template DRY | hasLegalMove generics | ✓✓✓ | ✓ |
| Incapsulamento | Bitboards private | ✓✓✓ | - |
| Costanti | Enum + constexpr | ✓✓ | - |

---

## Ordine Implementazione

1. **Estrazione FEN Parser** (basso rischio, già isolato in `fen.cpp`)
2. **Bitboard Manager** (riduce dipendenze cross-modulo)
3. **Compact MoveState** (impatto immediato su search)
4. **Move Validator** (richede testing estensivo)
5. **Index-only API** (refactoring incrementale)
6. **Dispatch table** (ultimo, richiede benchmark)

---

## Testing & Validazione

**Strategia**:
- Test parametrici con posizioni FEN note
- Benchmark isolati per ogni ottimizzazione
- Validazione vs suite Stockfish (`tests/perft`)
- Profiling con `perf stat -e cache-misses,branches,branch-misses`

**Criteri di successo**:
- 0 regressioni in test suite esistente
- Leggibilità: max 200 righe per file hpp
- Prestazioni: mantenere/migliorare velocità attuale
