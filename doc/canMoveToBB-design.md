# canMoveToBB - Design Document v2.0

## 📋 Executive Summary

**Obiettivo**: Riprogettare `canMoveToBB()` per massimizzare performance e leggibilità, sfruttando magic bitboards e funzioni helper modulari.

**Priorità**: **PERFORMANCE FIRST** - Leggibilità tramite decomposizione in funzioni helper inline aggressive.

---

## 🎯 Decisioni Architetturali

### 1. **Double-Check Detection**
- **Strategia**: **Lazy Evaluation (Opzione A)**
- **Implementazione**: Calcola double-check SOLO quando necessario (dopo primi check falliti)
- **Motivazione**: Evita calcolo costoso se mossa rifiutata per altri motivi (es. cattura pezzo proprio)

### 2. **En-Passant Storage**
- **Strategia**: **Single Coords** (non std::array)
- **Implementazione**: `Coords enPassant{}` - unica EP square valida
- **Motivazione**: Solo 1 en-passant possibile per posizione, validità via `Coords::isInBounds()`
- **Breaking Change**: Richiede refactoring di `Board::enPassant` da `std::array<Coords, 2>` a `Coords`

### 3. **Castling Logic**
- **Strategia**: **Funzione Helper Esterna**
- **Implementazione**: `bool canCastleKingside(...)` e `bool canCastleQueenside(...)`
- **Motivazione**: Riduce complessità switch-case KING da 80 righe a ~10 righe

### 4. **King Safety Check**
- **Strategia**: **Mantieni `isKingAttackedCustom`**
- **Implementazione**: Zero-copy bitboard simulation già ottimizzata
- **Motivazione**: Già massimamente efficiente

### 5. **Branch Prediction Hints**
- **Strategia**: **Aggressive `[[likely]]/[[unlikely]]`**
- **Implementazione**: Annotare tutti i branch critici con hint espliciti
- **Motivazione**: Aiuta CPU branch predictor per hot paths

---

## 🏗️ Architettura Proposta

### Nuova Struttura Funzionale

```
canMoveToBB()
├── [Fast Path] Early validation
│   ├── canCaptureOwnPiece()         ❌ inline
│   └── isDoubleCheck()              ⚡ lazy eval
│
├── [Switch] Pseudo-legal generation
│   ├── case PAWN   → isPawnMoveLegal()       🆕 helper
│   ├── case KNIGHT → isSimplePieceLegal()    🆕 helper
│   ├── case BISHOP → isSimplePieceLegal()    🆕 helper
│   ├── case ROOK   → isSimplePieceLegal()    🆕 helper
│   ├── case QUEEN  → isSimplePieceLegal()    🆕 helper
│   └── case KING   → isKingMoveLegal()       🆕 helper
│       ├── canCastleKingside()    🆕 helper
│       └── canCastleQueenside()   🆕 helper
│
└── [Final] King safety check
    └── verifyKingSafetyForSimplePiece()   🆕 helper
```

---

## 📝 Dettaglio Funzioni Helper

### 1. `isDoubleCheck()` - Lazy Evaluation
```cpp
// INLINE - called only when inChk=true && fromType != KING
[[nodiscard]] inline bool Board::isDoubleCheck(uint8_t movingColor) const noexcept {
    const uint8_t side = (movingColor == WHITE) ? 0 : 1;
    const uint8_t kingIndex = __builtin_ctzll(kings_bb[side]);
    const uint8_t oppSide = side ^ 1;
    
    uint64_t attackers = 0;
    
    // Fast path: non-sliding pieces
    attackers += __builtin_popcountll(pieces::PAWN_ATTACKERS_TO[oppSide][kingIndex] & pawns_bb[oppSide]);
    if (attackers >= 2) [[unlikely]] return true;
    
    attackers += __builtin_popcountll(pieces::KNIGHT_ATTACKS[kingIndex] & knights_bb[oppSide]);
    if (attackers >= 2) [[unlikely]] return true;
    
    attackers += __builtin_popcountll(pieces::KING_ATTACKS[kingIndex] & kings_bb[oppSide]);
    if (attackers >= 2) [[unlikely]] return true;
    
    // Sliding pieces
    attackers += __builtin_popcountll(pieces::getRookAttacks(kingIndex, occupancy) & 
                                      (rooks_bb[oppSide] | queens_bb[oppSide]));
    if (attackers >= 2) [[unlikely]] return true;
    
    attackers += __builtin_popcountll(pieces::getBishopAttacks(kingIndex, occupancy) & 
                                      (bishops_bb[oppSide] | queens_bb[oppSide]));
    
    return attackers >= 2;
}
```

**Performance**:
- ✅ Called ONLY when `inChk=true` && moving non-king piece
- ✅ Early exits on double-check detection
- ✅ Evita calcolo se mossa illegale per altri motivi

---

### 2. `isPawnMoveLegal()` - Pawn Logic + En-Passant Optimization
```cpp
// INLINE - handles pawn moves, captures, en-passant
[[nodiscard]] inline bool Board::isPawnMoveLegal(
    uint8_t fromIndex, 
    uint8_t toIndex,
    uint64_t toBit,
    uint8_t movingColor,
    uint8_t destPiece,
    uint8_t destColor
) const noexcept {
    const bool isWhite = (movingColor == WHITE);
    const uint8_t side = isWhite ? 0 : 1;
    const uint8_t oppSide = side ^ 1;
    const uint8_t oppColor = (movingColor == WHITE) ? BLACK : WHITE;
    
    // Generate attacks and pushes using magic bitboards
    const uint64_t attacks = pieces::PAWN_ATTACKS[isWhite][fromIndex];
    const uint64_t pushes  = pieces::getPawnForwardPushes(fromIndex, isWhite, occupancy);
    
    // Check if move is in legal set
    bool isEnPassant = false;
    
    // En-passant detection - OPTIMIZED with single Coords
    if ((attacks & toBit) && ((occupancy & toBit) == 0ULL)) [[unlikely]] {
        // Diagonal move into empty square → potential EP
        if (Coords::isInBounds(enPassant) && toIndex == enPassant.index) [[likely]] {
            isEnPassant = true;
        } else {
            return false; // Diagonal into empty non-EP square = illegal
        }
    }
    
    // Normal capture or push
    if (!isEnPassant) [[likely]] {
        const bool isCapture = (attacks & toBit) && ((occupancy & toBit) != 0ULL);
        const bool isPush = (pushes & toBit) && ((occupancy & toBit) == 0ULL);
        
        if (!isCapture && !isPush) [[unlikely]] return false;
    }
    
    // King safety check - simulate move with modified bitboards
    uint64_t occNew = occupancy;
    occNew &= ~(1ULL << fromIndex);
    occNew |= toBit;
    
    uint64_t excludeMask = 0ULL;
    
    if (isEnPassant) [[unlikely]] {
        const int8_t captureOffset = isWhite ? 8 : -8;
        const uint8_t capIndex = toIndex + captureOffset;
        excludeMask = (1ULL << capIndex);
        occNew &= ~excludeMask;
    } else if (destPiece != EMPTY && destColor == oppColor) [[likely]] {
        excludeMask = toBit;
    }
    
    const uint64_t kingBB = kings_bb[side];
    if (!kingBB) [[unlikely]] return false;
    const uint8_t kingSq = __builtin_ctzll(kingBB);
    
    // Zero-copy king safety check
    return !isKingAttackedCustom(kingSq, oppColor, occNew,
                                 pawns_bb[oppSide] & ~excludeMask,
                                 knights_bb[oppSide] & ~excludeMask,
                                 bishops_bb[oppSide] & ~excludeMask,
                                 rooks_bb[oppSide] & ~excludeMask,
                                 queens_bb[oppSide] & ~excludeMask,
                                 kings_bb[oppSide] & ~excludeMask);
}
```

**Ottimizzazioni En-Passant**:
- ✅ Single `Coords enPassant` invece di array
- ✅ Check diretto: `toIndex == enPassant.index`
- ✅ Branch hint `[[unlikely]]` per EP (raro)

---

### 3. `isSimplePieceLegal()` - Knight/Bishop/Rook/Queen
```cpp
// INLINE - handles simple sliding/jumping pieces
[[nodiscard]] inline bool Board::isSimplePieceLegal(
    uint64_t bitMap,
    uint64_t toBit
) const noexcept {
    // Just check if destination is in pseudo-legal bitboard
    return (bitMap & toBit) != 0ULL;
}
```

**Motivazione**:
- Knights/Bishops/Rooks/Queens: pseudo-legal check è sufficiente qui
- King safety verificato DOPO lo switch nel caller
- Minimizza duplicazione codice

---

### 4. `isKingMoveLegal()` - King + Castling
```cpp
// INLINE - handles king normal moves + castling delegation
[[nodiscard]] inline bool Board::isKingMoveLegal(
    uint8_t fromIndex,
    uint8_t toIndex,
    uint64_t toBit,
    uint8_t movingColor
) const noexcept {
    const uint64_t bitMap = pieces::KING_ATTACKS[fromIndex];
    
    // Normal king move: check if destination attacked
    if (fromIndex != toIndex) [[likely]] {
        const uint8_t oppColor = (movingColor == WHITE) ? BLACK : WHITE;
        if ((bitMap & toBit) && isSquareAttacked(toIndex, oppColor, fromIndex)) [[unlikely]] {
            // Destination attacked → check castling as fallback
            return canCastleToSquare(fromIndex, toIndex, movingColor);
        }
    }
    
    // Normal move is safe OR castling is legal
    return (bitMap & toBit) || canCastleToSquare(fromIndex, toIndex, movingColor);
}
```

---

### 5. `canCastleToSquare()` - Castling Dispatcher
```cpp
// INLINE - determines which castling helper to call
[[nodiscard]] inline bool Board::canCastleToSquare(
    uint8_t fromIndex,
    uint8_t toIndex,
    uint8_t movingColor
) const noexcept {
    const uint8_t fromRank = fromIndex >> 3;
    const uint8_t toRank = toIndex >> 3;
    
    // Same rank check
    if (fromRank != toRank) [[likely]] return false;
    
    const bool isWhite = (movingColor == WHITE);
    const int df = static_cast<int>(toIndex & 7) - static_cast<int>(fromIndex & 7);
    const uint8_t kf = fromIndex & 7;
    
    // Must be from initial king square
    if (!((isWhite && fromRank == 7 && kf == 4) || (!isWhite && fromRank == 0 && kf == 4))) [[likely]] {
        return false;
    }
    
    if (df == 2) [[unlikely]] return canCastleKingside(isWhite, fromIndex);
    if (df == -2) [[unlikely]] return canCastleQueenside(isWhite, fromIndex);
    
    return false;
}
```

---

### 6. `canCastleKingside()` - Kingside Castling
```cpp
// INLINE - kingside castling validation
[[nodiscard]] inline bool Board::canCastleKingside(bool isWhite, uint8_t fromIndex) const noexcept {
    const uint8_t side = isWhite ? 0 : 1;
    const uint8_t oppColor = isWhite ? BLACK : WHITE;
    
    // Check castling rights
    const bool hasRights = isWhite 
        ? ((castle & (1u << 0)) != 0u)  // White O-O
        : ((castle & (1u << 2)) != 0u); // Black O-O
    
    if (!hasRights) [[unlikely]] return false;
    
    // Check empty squares (f & g files)
    const uint8_t f1Idx = fromIndex + 1;
    const uint8_t f2Idx = fromIndex + 2;
    const uint8_t rookIdx = fromIndex + 3;
    
    if ((this->get(f1Idx) != EMPTY) || (this->get(f2Idx) != EMPTY)) [[unlikely]] {
        return false;
    }
    
    // Check rook presence
    if ((rooks_bb[side] & (1ULL << rookIdx)) == 0ULL) [[unlikely]] {
        return false;
    }
    
    // Check castle path safety (e, f, g squares)
    const uint64_t castlePath = (1ULL << fromIndex) | (1ULL << f1Idx) | (1ULL << f2Idx);
    return isCastlePathSafe(castlePath, oppColor);
}
```

---

### 7. `canCastleQueenside()` - Queenside Castling
```cpp
// INLINE - queenside castling validation
[[nodiscard]] inline bool Board::canCastleQueenside(bool isWhite, uint8_t fromIndex) const noexcept {
    const uint8_t side = isWhite ? 0 : 1;
    const uint8_t oppColor = isWhite ? BLACK : WHITE;
    
    // Check castling rights
    const bool hasRights = isWhite
        ? ((castle & (1u << 1)) != 0u)  // White O-O-O
        : ((castle & (1u << 3)) != 0u); // Black O-O-O
    
    if (!hasRights) [[unlikely]] return false;
    
    // Check empty squares (d, c, b files)
    const uint8_t d1Idx = fromIndex - 1;
    const uint8_t d2Idx = fromIndex - 2;
    const uint8_t d3Idx = fromIndex - 3;
    const uint8_t rookIdx = fromIndex - 4;
    
    if ((this->get(d1Idx) != EMPTY) || (this->get(d2Idx) != EMPTY) || (this->get(d3Idx) != EMPTY)) [[unlikely]] {
        return false;
    }
    
    // Check rook presence
    if ((rooks_bb[side] & (1ULL << rookIdx)) == 0ULL) [[unlikely]] {
        return false;
    }
    
    // Check castle path safety (e, d, c squares)
    const uint64_t castlePath = (1ULL << fromIndex) | (1ULL << d1Idx) | (1ULL << d2Idx);
    return isCastlePathSafe(castlePath, oppColor);
}
```

---

### 8. `verifyKingSafetyForSimplePiece()` - Final Pin Check
```cpp
// INLINE - king safety for non-king, non-pawn pieces
[[nodiscard]] inline bool Board::verifyKingSafetyForSimplePiece(
    uint8_t fromIndex,
    uint8_t toIndex,
    uint8_t movingColor,
    uint8_t destPiece,
    uint8_t destColor
) const noexcept {
    const uint8_t side = (movingColor == WHITE) ? 0 : 1;
    const uint8_t oppSide = side ^ 1;
    const uint8_t oppColor = (movingColor == WHITE) ? BLACK : WHITE;
    
    // Simulate move
    uint64_t occNew = occupancy;
    occNew &= ~(1ULL << fromIndex);
    occNew |= (1ULL << toIndex);
    
    // King square (unchanged)
    const uint64_t kingBB = kings_bb[side];
    if (!kingBB) [[unlikely]] return false;
    const uint8_t kingSq = __builtin_ctzll(kingBB);
    
    // Exclusion mask for captured piece
    const uint64_t excludeMask = (destPiece != EMPTY && destColor == oppColor) 
        ? (1ULL << toIndex) 
        : 0ULL;
    
    // Zero-copy king safety check
    return !isKingAttackedCustom(kingSq, oppColor, occNew,
                                 pawns_bb[oppSide] & ~excludeMask,
                                 knights_bb[oppSide] & ~excludeMask,
                                 bishops_bb[oppSide] & ~excludeMask,
                                 rooks_bb[oppSide] & ~excludeMask,
                                 queens_bb[oppSide] & ~excludeMask,
                                 kings_bb[oppSide] & ~excludeMask);
}
```

---

## 🚀 Nuova Implementazione `canMoveToBB()`

```cpp
bool Board::canMoveToBB(const Coords& from, const Coords& to, bool inChk) const noexcept {
    // ============================================
    // PHASE 1: FAST PATH - Early Validation
    // ============================================
    const uint8_t fromIndex = from.index;
    const uint8_t toIndex = to.index;
    const uint64_t toBit = (1ULL << toIndex);
    
    const uint8_t fromType = this->get(from) & this->MASK_PIECE_TYPE;
    const uint8_t movingColor = this->getColor(from);
    const uint8_t oppColor = (movingColor == WHITE) ? BLACK : WHITE;
    
    const uint8_t destPiece = this->get(to);
    const uint8_t destColor = destPiece & MASK_COLOR;
    
    // Early exit: can't capture own piece
    if (destPiece != EMPTY && destColor == movingColor) [[unlikely]] {
        return false;
    }
    
    // Lazy double-check evaluation
    if (inChk && fromType != KING) [[unlikely]] {
        if (isDoubleCheck(movingColor)) [[unlikely]] {
            return false; // Only king moves allowed in double-check
        }
    }
    
    // ============================================
    // PHASE 2: PSEUDO-LEGAL GENERATION + VALIDATION
    // ============================================
    switch (fromType) {
        case PAWN:
            return isPawnMoveLegal(fromIndex, toIndex, toBit, movingColor, destPiece, destColor);
        
        case KNIGHT: {
            const uint64_t bitMap = pieces::KNIGHT_ATTACKS[fromIndex];
            if (!isSimplePieceLegal(bitMap, toBit)) [[unlikely]] return false;
            return verifyKingSafetyForSimplePiece(fromIndex, toIndex, movingColor, destPiece, destColor);
        }
        
        case BISHOP: {
            const uint64_t bitMap = pieces::getBishopAttacks(fromIndex, occupancy);
            if (!isSimplePieceLegal(bitMap, toBit)) [[unlikely]] return false;
            return verifyKingSafetyForSimplePiece(fromIndex, toIndex, movingColor, destPiece, destColor);
        }
        
        case ROOK: {
            const uint64_t bitMap = pieces::getRookAttacks(fromIndex, occupancy);
            if (!isSimplePieceLegal(bitMap, toBit)) [[unlikely]] return false;
            return verifyKingSafetyForSimplePiece(fromIndex, toIndex, movingColor, destPiece, destColor);
        }
        
        case QUEEN: {
            const uint64_t bitMap = pieces::getQueenAttacks(fromIndex, occupancy);
            if (!isSimplePieceLegal(bitMap, toBit)) [[unlikely]] return false;
            return verifyKingSafetyForSimplePiece(fromIndex, toIndex, movingColor, destPiece, destColor);
        }
        
        case KING:
            return isKingMoveLegal(fromIndex, toIndex, toBit, movingColor);
        
        [[unlikely]] default:
            return false;
    }
}
```

---

## 📊 Vantaggi della Nuova Architettura

### Performance Improvements

| Ottimizzazione | Beneficio | Impatto |
|----------------|-----------|---------|
| Lazy double-check | Skip calcolo costoso se mossa illegale per altri motivi | **ALTO** |
| Single Coords EP | -1 array access, confronto diretto con index | **MEDIO** |
| Inline helpers | Zero overhead chiamata, aggressive inlining | **ALTO** |
| Branch hints | CPU branch predictor ottimizzato | **MEDIO** |
| Castling extraction | Riduce instruction cache pressure | **MEDIO** |
| Early exits | Skip validazioni inutili | **ALTO** |

### Leggibilità Improvements

| Aspetto | Prima | Dopo |
|---------|-------|------|
| Lunghezza `canMoveToBB()` | ~200 righe | ~60 righe |
| KING case complexity | ~80 righe | ~10 righe |
| PAWN logic | Embedded inline | Funzione dedicata |
| Castling logic | Inline nel switch | 3 funzioni helper |
| Test coverage | Difficile | Facile (test helpers) |

---

## 🔧 Migration Steps

### Step 1: Board Storage Refactoring
```cpp
// board.hpp - PRIMA
private:
    std::array<Coords, 2> enPassant = {Coords{}, Coords{}};

// board.hpp - DOPO
private:
    Coords enPassant{}; // Single en-passant square
```

### Step 2: Update moveBB() en-passant logic
```cpp
// board.cpp - moveBB() - PRIMA
Coords prevEp = enPassant[0];
enPassant[0] = Coords{};
enPassant[1] = Coords{};

// board.cpp - moveBB() - DOPO
Coords prevEp = enPassant;
enPassant = Coords{}; // Invalidate

// ... later in moveBB() ...
if (dr == 2 || dr == -2) {
    const uint8_t midIndex = (fromIndex + toIndex) >> 1;
    enPassant = Coords{midIndex}; // Set EP square
}
```

### Step 3: Update FEN parsing/generation
```cpp
// Rimuovi logica [0]/[1] indexing, usa direttamente enPassant
```

### Step 4: Add helper functions to board.hpp
```cpp
// Dichiarazioni inline helpers in board.hpp (private section)
[[nodiscard]] inline bool isDoubleCheck(uint8_t movingColor) const noexcept;
[[nodiscard]] inline bool isPawnMoveLegal(...) const noexcept;
[[nodiscard]] inline bool isSimplePieceLegal(...) const noexcept;
[[nodiscard]] inline bool isKingMoveLegal(...) const noexcept;
[[nodiscard]] inline bool canCastleToSquare(...) const noexcept;
[[nodiscard]] inline bool canCastleKingside(...) const noexcept;
[[nodiscard]] inline bool canCastleQueenside(...) const noexcept;
[[nodiscard]] inline bool verifyKingSafetyForSimplePiece(...) const noexcept;
```

### Step 5: Implement helpers in board.cpp
- Implementare tutte le funzioni helper come descritto sopra
- Aggiungere `[[nodiscard]]` e branch hints

### Step 6: Replace canMoveToBB() implementation
- Sostituire implementazione attuale con nuova versione modulare

### Step 7: Testing
```bash
make clean && make
./tests  # Verify all tests pass
```

---

## ⚠️ Breaking Changes

1. **Board::enPassant** cambia da `std::array<Coords, 2>` a `Coords`
   - Impatto: FEN parsing/generation, doMove/undoMove
   - Fix: Rimuovi `[0]` indexing, usa direttamente `enPassant`

2. **Nuove funzioni helper private in Board class**
   - Impatto: Nessuno (internal only)
   - Fix: Nessuno

---

## 🎯 Expected Performance Gains

### Benchmark Targets (vs current implementation)

- **Normal moves**: ~5-10% faster (early exits, lazy evaluation)
- **Pawn moves**: ~10-15% faster (single Coords EP, optimized flow)
- **King moves**: ~20-30% faster (castling extraction, cleaner logic)
- **Double-check**: ~30-40% faster (lazy evaluation, skip quando inutile)
- **Code size**: ~30% reduction (helper extraction, less duplication)

### Memory Impact
- **Stack usage**: Neutral (inline helpers)
- **Code cache**: Improved (smaller functions, better locality)
- **Heap**: Reduced by 1 Coords (array → single)

---

## ✅ Validation Checklist

- [ ] Compile senza errori/warning
- [ ] Tutti i test unitari passano
- [ ] Performance benchmark mostra miglioramenti
- [ ] Code coverage >= 95% per nuovi helpers
- [ ] Nessuna regressione in engine strength (Elo test)
- [ ] Valgrind: zero memory leaks
- [ ] Perft tests match reference implementation

---

## 📚 References

- Magic Bitboards: Already implemented in `piece/piece.hpp`
- Coords convention: Index 0=a8, 63=h1 (rank increases downward)
- Castling rights: bits [0]=K, [1]=Q, [2]=k, [3]=q
- Branch hints: https://en.cppreference.com/w/cpp/language/attributes/likely

---

**Design Version**: 2.0  
**Date**: 2026-01-05  
**Author**: Chess Engine Refactoring Team  
**Status**: ✅ READY FOR IMPLEMENTATION
