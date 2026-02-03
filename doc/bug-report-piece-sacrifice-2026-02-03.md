# 🐛 BUG REPORT - Engine Sacrifica Pezzi a Caso

**Data:** 3 Febbraio 2026  
**Versione:** beta0.1.0  
**Problema:** L'engine sacrifica pezzi in posizioni vincenti senza compensazione

---

## 📋 Sommario Esecutivo

L'engine presenta un comportamento critico dove sacrifica pezzi (anche la Regina) in posizioni vincenti senza alcuna compensazione tattica. Dopo analisi approfondita del codice di valutazione e ricerca, sono stati identificati **3 bug critici** che interagiscono tra loro causando il problema.

**Root Cause:** Combinazione di:
1. SEE (Static Exchange Evaluation) con early-exit troppo aggressivo che marca catture buone come perdenti
2. Material contempt troppo debole per prevenire sacrifici speculativi
3. Quiescence search che salta catture buone a causa del SEE errato

---

## 🚨 BUG CRITICO #1: SEE Early-Exit Troppo Aggressivo

### Localizzazione
**File:** `engine/search.cpp`  
**Linea:** 793  
**Funzione:** `staticExchangeEvaluation()`

### Codice Problematico
```cpp
// EARLY-EXIT: se il pezzo catturato vale significativamente meno di
// quello che sta effettuando la cattura (es. QxP), nella maggior parte
// dei casi la cattura è perdente dopo le riprese; saltiamo il SEE costoso
// e ritorniamo una stima negativa rapida.
// Soglia: un pedone (PAWN_VALUE) per evitare falsi positivi su scambi
// ravvicinati. Usiamo la costante PAWN_VALUE per rendere esplicite le unità.
if (PIECE_VALUES[capturedType] < PIECE_VALUES[capturedOnTargetType] - PAWN_VALUE * 2) {
    return static_cast<int64_t>(PIECE_VALUES[capturedType] - PIECE_VALUES[capturedOnTargetType]);
}
```

### Problema
La soglia di `-PAWN_VALUE * 2` (200 centipawn) è **TROPPO GRANDE** e causa **falsi negativi** per molte catture normalmente buone.

**Variabili:**
- `capturedType` = pezzo CATTURATO (vittima sulla casella `to`)
- `capturedOnTargetType` = pezzo CHE CATTURA (attaccante che si muove da `from`)

### Esempi di Catture Errate

| Cattura | Vittima (cp) | Attaccante (cp) | Condizione | SEE Ritornato | SEE Corretto | Errore |
|---------|--------------|-----------------|------------|---------------|--------------|--------|
| **QxP** | 100 | 900 | `100 < 700` ✅ | `-800` | `-800` | ✅ OK |
| **QxR** | 500 | 900 | `500 < 700` ✅ | **`-400`** | **`+400`** | ❌ **CRITICO** |
| **QxN** | 320 | 900 | `320 < 700` ✅ | **`-580`** | **`+320`** | ❌ **CRITICO** |
| **QxB** | 330 | 900 | `330 < 700` ✅ | **`-570`** | **`+330`** | ❌ **CRITICO** |
| **RxP** | 100 | 500 | `100 < 300` ✅ | **`-400`** | variabile | ❌ **ERRORE** |
| **RxN** | 320 | 500 | `320 < 300` ❌ | calcola SEE | calcola SEE | ✅ OK |
| **BxN** | 320 | 330 | `320 < 130` ❌ | calcola SEE | calcola SEE | ✅ OK |

### Conseguenze
1. **QxR** (Regina cattura Torre) viene valutato come **perdente** (`-400`) invece di **vincente** (`+400`)
2. **QxN**, **QxB** similmente valutate come perdenti quando in realtà vincono materiale
3. L'engine **NON VEDE** catture materialmente vantaggiose
4. In quiescence search, catture con SEE < -300 vengono **SKIPPATE** completamente
5. L'engine pensa di non avere difese e fa sacrifici disperati

### Fix Proposto
```cpp
// EARLY-EXIT: Solo per catture OVVIAMENTE perdenti (es. QxP con riconquista garantita)
// Soglia MOLTO conservativa: solo se vittima < attaccante - 4 pedoni (400cp)
// Questo elimina solo QxP, QxN quando l'attaccante è Queen o Rook
if (PIECE_VALUES[capturedType] + 400 < PIECE_VALUES[capturedOnTargetType]) {
    // Esempio: QxP → 100 + 400 < 900 → TRUE (skip SEE, ritorna -800)
    // Esempio: QxR → 500 + 400 < 900 → FALSE (calcola SEE completo!)
    // Esempio: RxP → 100 + 400 < 500 → FALSE (calcola SEE)
    return static_cast<int64_t>(PIECE_VALUES[capturedType] - PIECE_VALUES[capturedOnTargetType]);
}
```

**Alternativa:** Rimuovere completamente l'early-exit se la performance lo consente.

---

## 🚨 BUG CRITICO #2: Material Contempt Troppo Debole

### Localizzazione
**File:** `engine/evaluate.cpp`  
**Linee:** 1399-1427  
**Funzione:** `evaluatePosition()`

### Codice Problematico
```cpp
// MATERIAL CONTEMPT - Discourage speculative sacrifices
const int64_t matDelta = getMaterialDelta(board);
const int64_t absMatDelta = std::abs(matDelta);

// Only apply contempt if material difference is significant (> 1.5 pawns = 150cp)
if (absMatDelta > 150) {
    // Check if the losing side is giving check (might indicate mating attack)
    const bool loserGivingCheck = (matDelta > 0) 
        ? board.inCheck(chess::Board::WHITE)  // White ahead, check if Black checking
        : board.inCheck(chess::Board::BLACK); // Black ahead, check if White checking
    
    if (!loserGivingCheck) {
        // Apply contempt: extra penalty proportional to material loss
        // Formula: 20% of the material loss as extra penalty
        const int64_t contemptPenalty = absMatDelta / 5; // 20%
        
        // Apply from White's perspective
        eval += (matDelta > 0) ? contemptPenalty : -contemptPenalty;
    }
}
```

### Problema
La penalità del **20%** è **INSUFFICIENTE** per scoraggiare sacrifici di pezzi pesanti.

### Analisi Quantitativa

| Sacrificio | Delta Materiale | Soglia Attivazione | Penalità (20%) | Penalità Totale | Efficacia |
|------------|-----------------|--------------------| ---------------|-----------------|-----------|
| 1 Pedone | -100 cp | ❌ NO (< 150) | 0 | **-100** | ❌ **Nessuna protezione** |
| 2 Pedoni | -200 cp | ✅ SI | -40 | **-240** | ⚠️ **Debole** |
| Cavallo | -320 cp | ✅ SI | -64 | **-384** | ⚠️ **Debole** |
| Torre | -500 cp | ✅ SI | -100 | **-600** | ⚠️ **Borderline** |
| **Regina** | **-900 cp** | ✅ SI | **-180** | **-1080** | ❌ **INSUFFICIENTE** |

### Scenario Critico
Un sacrificio di Regina riceve solo **-180 cp** di penalità aggiuntiva:

```
Valutazione sacrificio Regina per attacco:
  Material loss:        -900 cp
  Material contempt:    -180 cp  (20%)
  ────────────────────────────
  Subtotal:            -1080 cp
  
  Valutazione tattica:  +250 cp  (mobilità, re esposto, minacce)
  ────────────────────────────
  TOTAL:                -830 cp
```

Se l'engine valuta l'attacco tattico come +250 cp (possibile con mobilità bonus, re esposto, minacce percepite), potrebbe considerare il sacrificio **accettabile** perché "almeno compensato parzialmente".

### Fix Proposto
```cpp
// Apply STRONG contempt to discourage speculative sacrifices
// Use 50% penalty for moderate losses, 100% for large losses
int64_t contemptPenalty;
if (absMatDelta < 300) {
    // Small material loss (< 3 pawns): 50% penalty
    contemptPenalty = absMatDelta / 2;
} else {
    // Large material loss (>= 3 pawns): 100% penalty
    // This makes Queen sacrifices VERY expensive:
    // -900 material + -900 contempt = -1800 total penalty
    contemptPenalty = absMatDelta;
}

// Apply from White's perspective
eval += (matDelta > 0) ? contemptPenalty : -contemptPenalty;
```

**Risultato dopo fix:**
- Sacrificio Torre: -500 - 500 = **-1000 cp** (richiede +1000 di compensazione)
- Sacrificio Regina: -900 - 900 = **-1800 cp** (quasi impossibile compensare)

---

## 🔍 Interazione dei Bug (Effetto Combinato)

### Catena di Eventi che Causa i Sacrifici

1. **Posizione iniziale:** Engine in vantaggio materiale (+300 cp)

2. **Bug #1 (SEE):** QxR viene valutato come `-400` invece di `+400`
   ```
   SEE(QxR) = -400  (SBAGLIATO!)
   ```

3. **Quiescence Search:** Salta la cattura perché SEE < -300
   ```cpp
   if (see <= SEE_HARD_CUTOFF) {  // -400 <= -300
       continue;  // ❌ SKIP! Non vede la difesa!
   }
   ```

4. **Engine non vede difese:** Pensa di perdere materiale comunque

5. **Bug #2 (Material Contempt debole):** Sacrificio "disperato" ha penalità troppo bassa
   ```
   Sacrificio Regina: -900 - 180 = -1080
   + Valutazione tattica: +200
   = -880 (sembra "meno peggio" che subire l'attacco nemico)
   ```

6. **Risultato:** Engine sacrifica la Regina pensando sia la "meno peggio" delle opzioni

---

## 🛠️ Piano di Fix

### Fix Urgenti (Priorità ALTA)

#### 1. Fix SEE Early-Exit
**File:** `engine/search.cpp:793`  
**Azione:** Cambiare soglia da `- PAWN_VALUE * 2` (200) a `+ 400` per evitare falsi negativi

```cpp
// OLD (BUGGY):
if (PIECE_VALUES[capturedType] < PIECE_VALUES[capturedOnTargetType] - PAWN_VALUE * 2) {

// NEW (FIXED):
if (PIECE_VALUES[capturedType] + 400 < PIECE_VALUES[capturedOnTargetType]) {
```

#### 2. Rafforzare Material Contempt
**File:** `engine/evaluate.cpp:1420`  
**Azione:** Aumentare penalità da 20% a 50-100% scala progressiva

```cpp
// OLD (BUGGY):
const int64_t contemptPenalty = absMatDelta / 5; // 20%

// NEW (FIXED):
int64_t contemptPenalty;
if (absMatDelta < 300) {
    contemptPenalty = absMatDelta / 2; // 50% for small losses
} else {
    contemptPenalty = absMatDelta;     // 100% for large losses
}
```

### Testing Plan

Dopo i fix, testare con:
1. Posizioni dove l'engine faceva sacrifici assurdi
2. Verificare che QxR, QxN, QxB siano valutate correttamente
3. Test performance (il SEE completo è più lento dell'early-exit)
4. Verificare che material contempt non sia TROPPO forte (bloccherebbe sacrifici validi)

---

## 📊 Valori di Riferimento

### Piece Values
```cpp
PAWN_VALUE   = 100
KNIGHT_VALUE = 320
BISHOP_VALUE = 330
ROOK_VALUE   = 500
QUEEN_VALUE  = 900
KING_VALUE   = 20000
```

### SEE Thresholds in Quiescence
```cpp
seeThreshold = (ply < 20) ? -16 : 0;      // Dynamic threshold
SEE_HARD_CUTOFF = -300;                    // Absolute cutoff
```

### Material Contempt (Current)
```cpp
Activation threshold: absMatDelta > 150
Penalty formula: absMatDelta / 5  (20%)
Skip if loser giving check
```

---

## ✅ Conclusioni

I bug trovati spiegano completamente il comportamento dell'engine:

1. **SEE errato** → engine non vede catture difensive buone
2. **Material contempt debole** → sacrifici non scoraggiati abbastanza
3. **Combinazione letale** → engine sacrifica pensando di essere perdente comunque

**Fix applicati (2026-02-03):**
1. ✅ SEE early-exit: soglia corretta da -200cp a +400cp
2. ✅ Material contempt: penalty rafforzata da 20% a 50-100% progressivo
3. ✅ Material contempt threshold: abbassata da 150cp a 50cp
4. ✅ Quiescence: rimosso HARD_CUTOFF ridondante (-300cp), ora usa solo seeThreshold dinamico (-16cp/0cp)

**Fix attesi:** Dopo correzione dei bug, l'engine dovrebbe:
- Vedere correttamente catture come QxR, QxN, QxB come vantaggiose
- Evitare sacrifici speculativi senza compensazione concreta
- Giocare in modo più solido in posizioni vincenti
- Maggiore selettività nelle catture in quiescence search

**Rischio regression:** Il SEE completo è più lento dell'early-exit. Monitorare performance dopo il fix.
