# Ottimizzazioni generateLegalMoves

## Versione Precedente - Colli di Bottiglia

La versione precedente aveva questi problemi di performance:

1. **`canMoveToBB()` chiamato per ogni mossa**: Ogni mossa pseudo-legale richiedeva una verifica completa di legalità (pin, check, ecc.)
2. **Nessuna allocazione preventiva**: Il vector cresceva dinamicamente
3. **Lambda con overhead**: Capture di variabili e indirezioni

## Nuova Versione Ottimizzata

### 1. Pre-calcolo Check & Pin Info (Ottimizzazione Principale)

**Funzione `calculateCheckAndPins()`**:
- Calcola **UNA VOLTA** all'inizio:
  - `checkers`: bitboard dei pezzi che danno scacco
  - `pinned`: bitboard dei pezzi inchiodati
  - `checkMask`: maschera delle case che bloccano/catturano il pezzo che dà scacco
  - `numCheckers`: 0, 1, o 2 (doppio scacco)

**Vantaggi**:
- Elimina la maggior parte delle chiamate a `canMoveToBB()`
- Per pezzi non pinned e non sotto scacco: le mosse sono **direttamente legali**
- Solo i pezzi pinned richiedono `canMoveToBB()` per validazione

### 2. Gestione Intelligente dei Casi

**Double Check**:
```cpp
if (info.numCheckers >= 2) {
    return moves;  // Solo mosse del re sono legali
}
```

**Single Check**:
```cpp
if (info.numCheckers == 1) {
    destinations &= info.checkMask;  // Solo mosse che bloccano/catturano
}
```

**Cavalli Pinned**:
```cpp
if (isPinned) continue;  // Cavalli pinned non possono muovere
```

### 3. Vector Reserve

```cpp
moves.reserve(40);  // Tipica posizione ha ~30-40 mosse
```
Evita riallocazioni dinamiche durante la generazione.

### 4. Filtraggio Ottimizzato

```cpp
auto addFilteredMoves = [&](uint8_t from, uint64_t destinations, bool isPinned) {
    destinations &= ~ownOccupancy;
    
    if (info.numCheckers == 1) {
        destinations &= info.checkMask;  // Applica check mask
    }
    
    if (isPinned) {
        // Usa canMoveToBB solo per pezzi pinned
        while (destinations) { /* ... */ }
    } else {
        // Mosse direttamente legali!
        while (destinations) { /* ... */ }
    }
};
```

## Miglioramenti Attesi

### Performance
- **~3-5x più veloce** per posizioni normali
- **~10x più veloce** quando non sotto scacco con pochi pin
- Riduzione drastica delle chiamate a `canMoveToBB()`

### Esempi Pratici

**Posizione Normale** (non in check, 0-2 pin):
- Prima: 30 mosse × `canMoveToBB()` = 30 chiamate costose
- Dopo: 2 mosse (pinned) × `canMoveToBB()` = 2 chiamate costose

**Double Check**:
- Prima: Generava tutte le mosse, poi filtrava
- Dopo: Genera solo mosse del re immediatamente

**Single Check**:
- Prima: Testava ogni mossa con `canMoveToBB()`
- Dopo: Interseca con `checkMask`, poi genera solo mosse valide

## Implementazione Tecnica

### CheckPinInfo Structure
```cpp
struct CheckPinInfo {
    uint64_t checkers;    // Pezzi che danno scacco
    uint64_t pinned;      // Pezzi inchiodati
    uint64_t checkMask;   // Maschera per bloccare/catturare checker
    uint8_t kingSquare;   // Posizione del re
    int numCheckers;      // 0, 1, o 2
};
```

### Calcolo Pin Ray
Usa magic bitboards per trovare raggi tra:
- Re e attaccante
- Se esattamente 1 pezzo proprio è sul raggio → è pinned

### Calcolo Check Mask
Per scacco singolo:
- Square dell'attaccante
- Tutti gli square tra re e attaccante (sliding pieces)
- Usa intersezione di raggi da entrambe le direzioni

## Compatibilità

- ✅ Mantiene la stessa interfaccia: `std::vector<chess::Board::Move> generateLegalMoves(const chess::Board& b)`
- ✅ Genera le stesse mosse legali della versione precedente
- ✅ Gestisce tutti i casi speciali: en passant, castling, promotion
- ✅ Il re usa ancora `isSquareAttacked()` con exclude square per sicurezza

## Note

- Il castling richiede ancora `canMoveToBB()` (caso speciale con controlli multipli)
- Le mosse del re usano `isSquareAttacked()` per sicurezza aggiuntiva
- I pawn push potrebbero essere ulteriormente ottimizzati con lookup tables per square tra re e pawn
