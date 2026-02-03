# ANALISI APPROFONDITA: MOVE ORDERING
**Data**: 2026-02-03  
**File analizzati**: `engine/search.cpp`, `engine/basebonuspenaltyvalues.hpp`

---

## 📊 STATO ATTUALE DEL MOVE ORDERING

### Priorità Corrente (dalla più alta alla più bassa):

```
1. Hash Move (TT)         → 100000
2. Good Captures (SEE≥0)  → 10000 + MVV-LVA (0-9000) = 10000-19000
3. Killer Move 1          → 9000
4. Killer Move 2          → 8500
5. Checks (non-capture)   → 8000  [COMMENTATO - Non implementato]
6. Promotions             → 7000
7. History Heuristic      → 0-1000 (clamped)
8. Bad Captures (SEE<0)   → da -10000 a -30000 (gradato)
```

---

## 🐛 PROBLEMI CRITICI IDENTIFICATI

### 1. **CHECKS COMPLETAMENTE DISABILITATI** ⚠️ CRITICO
**Problema**: Il bonus per le mosse di scacco (8000 punti) è dichiarato ma **MAI USATO**.
```cpp
// Line 934: OPTIMIZATION: remove check detection from move ordering
// It is expensive (doMove/undoMove per quiet move)
// Checks will still be explored early because of killer moves and LMR
```

**Conseguenze**:
- Le mosse che danno scacco (non catture) vengono ordinate SOLO tramite history heuristic (0-1000)
- Scacchi forti in posizioni tattiche vengono esplorati tardi
- LMR può ridurre mosse di scacco cruciali prima di valutarle

**Soluzioni possibili**:
- **A) RIMUOVERE completamente** CHECK_BONUS (50 punti) da basebonuspenaltyvalues.hpp
- **B) IMPLEMENTARE in modo LAZY**: Check detection solo per le prime N mosse (top 5-10)
- **C) IMPLEMENTARE con BITBOARD**: Usa pre-calcolo attacchi invece di doMove/undoMove

**Raccomandazione**: Opzione **B** (lazy check detection) - bilancia performance/tattica

---

### 2. **KILLER MOVES: COLLISIONI E DUPLICATI**
**Problema**: Non c'è controllo per evitare che `killerMoves[0][ply] == killerMoves[1][ply]`

**Situazione attuale** (updateKillerAndHistoryOnBetaCutoff - non mostrato completamente):
```cpp
// Se la stessa mossa causa cutoff 2 volte consecutive:
killerMoves[0][ply] = m;  // Prima volta
killerMoves[1][ply] = killerMoves[0][ply]; // Shift
killerMoves[0][ply] = m;  // Seconda volta -> DUPLICATO!
```

**Conseguenze**:
- Spreco di uno slot killer (abbiamo 2 slot ma uno è duplicato)
- Riduce la diversità delle mosse con alta priorità

**Fix suggerito**:
```cpp
void Engine::updateKillerAndHistoryOnBetaCutoff(...) {
    // Non aggiungere se è già killer
    if (m.from.index == killerMoves[0][ply].from.index && 
        m.to.index == killerMoves[0][ply].to.index) {
        return; // Già killer primario
    }
    
    if (m.from.index == killerMoves[1][ply].from.index && 
        m.to.index == killerMoves[1][ply].to.index) {
        // Promuovi da killer2 a killer1
        killerMoves[1][ply] = killerMoves[0][ply];
        killerMoves[0][ply] = m;
        return;
    }
    
    // Shift normale
    killerMoves[1][ply] = killerMoves[0][ply];
    killerMoves[0][ply] = m;
}
```

---

### 3. **HISTORY HEURISTIC: SCALING LIMITATO**
**Problema**: History è clampato a [0, 1000] ma non c'è saturazione o decay intelligente

**Situazione**:
```cpp
// Line 959: history[colorIndex][m.from.index][m.to.index];
// Clampiamo a [0, 1000] per evitare valori anomali
score = std::min(static_cast<int64_t>(1000), std::max(static_cast<int64_t>(0), histScore));
```

**Problemi**:
- Se history raggiunge 1000, ogni mossa con history=1000 ha la stessa priorità
- Non distingue mosse "molto buone" da "estremamente buone"
- Decay è globale (divide tutto per 2 ogni search), non age-based per ply

**Ottimizzazioni suggerite**:
```cpp
// 1. Aumentare range: 0-2000 invece di 0-1000
// 2. Butterfly history: history[color][fromTo_hash] (riduce memoria)
// 3. Counter-move history: history[prevMove][currentMove] (migliora sequenze)
```

---

### 4. **MVV-LVA: PAWN CAPTURES SOTTOVALUTATE**
**Tabella corrente**:
```
PxP = 100*10 - 100 = 900
NxP = 100*10 - 320 = 680  ⚠️ KNIGHT cattura PAWN ha priorità MINORE di PxP!
BxP = 100*10 - 330 = 670  ⚠️ Anche BISHOP
```

**Problema**: MVV-LVA penalizza pezzi costosi che catturano pedoni
- `PxP` (900) ha priorità **maggiore** di `NxP` (680)
- Tatticamente sbagliato: preferisco esplorare `NxP` prima di `PxP`

**Fix suggerito** (formula alternativa):
```cpp
// Invece di: victimValue*10 - attackerValue
// Usare: victimValue*10 + (10 - attackerValue/100)
// Esempio: NxP = 1000 + (10 - 3.2) = 1006.8 > PxP = 1000 + (10 - 1) = 1009
// Oppure: victimValue * 100 + (100 - attackerValue)
// NxP = 10000 + (100-320) = 9780  vs  PxP = 10000 + (100-100) = 10000
```

**Meglio ancora** (solo victimValue, attacker irrilevante):
```cpp
// MVV-only (Most Valuable Victim)
score = PIECE_VALUES[victimType] * 10;
// Poi usa SEE per filtrare catture perdenti
```

---

### 5. **BAD CAPTURES: TROPPO GRANULARI?**
**Codice corrente**:
```cpp
if (see <= -200) {
    score = -30000 + see;  // see=-250 → -30250
} else if (see <= -50) {
    score = -20000 + see;  // see=-100 → -20100
} else {
    score = -10000 + see - 2000; // see=-10 → -12010
}
```

**Problema**: Tre tier sono eccessivi per catture perdenti
- Complessità aggiuntiva senza benefici evidenti
- Tutte le bad captures vengono esplorate comunque (dopo quiet)

**Semplificazione**:
```cpp
if (see < 0) {
    score = -10000 + see; // Priorità bassa, ordinamento per SEE
}
```

---

### 6. **HASH MOVE: NESSUNA VALIDAZIONE**
**Codice corrente**:
```cpp
// Line 862: Probe TT to get hash move
this->tt.probe(hashKey, 0, NEG_INF, POS_INF, dummyScore, encodedHashMove);

if (encodedHashMove != 0) {
    tt::TranspositionTable::Entry::decodeMove(encodedHashMove, hashFrom, hashTo, hashPromo);
}

// Line 888: Check if this is the hash move
if (m.from.index == hashFrom && m.to.index == hashTo && m.promotionPiece == hashPromo) {
    score = 100000; // Highest priority
}
```

**Problema**: Non verifica che `hashMove` sia **legale** nella posizione corrente
- Se TT ha collision (stessa hash, posizione diversa), hash move può essere illegale
- `sortLegalMoves` riceve `moves` già generati → hash move deve essere in `moves`

**Possibili scenari**:
- Hash move illegale → nessuna mossa riceve 100000 → OK ma spreco di probe
- Hash move semi-legale (promozione diversa) → potrebbe matchare parzialmente

**Fix**:
```cpp
bool hashMoveFound = false;
for (const auto& m : moves) {
    if (m.from.index == hashFrom && m.to.index == hashTo && m.promotionPiece == hashPromo) {
        hashMoveFound = true;
        break;
    }
}

// Poi usa hashMoveFound per assegnare score
```

---

### 7. **QUIESCENCE MOVE ORDERING: TROPPO SEMPLICE**
**Codice corrente** (line 1190):
```cpp
// Score by MVV-LVA for good ordering
score = 10000;
addMVVLVABonus(m, b, score);
```

**Problema**: Quiescence usa SOLO MVV-LVA, ignora:
- Hash move dalla TT (potrebbe esserci anche in qsearch!)
- SEE values per ordering (solo usa SEE per pruning)

**Miglioramenti**:
```cpp
// 1. Check hash move anche in quiescence
// 2. Ordinare per SEE value (non solo MVV-LVA):
score = 10000 + see; // Captures con SEE migliore esplorate prima
```

---

## 🚀 OTTIMIZZAZIONI PROPOSTE

### **PRIORITÀ ALTA** (Impatto Immediato)

#### 1. **Implementare Lazy Check Detection**
```cpp
// In sortLegalMoves(), dopo killer moves:
if (!isKiller && moveIndex < 8) { // Solo per prime 8 mosse
    chess::Board::MoveState tmpState;
    b.doMove(m, tmpState, isPromotionMove(b, m) ? 'q' : 0);
    if (b.inCheck(b.getActiveColor() == chess::Board::WHITE ? chess::Board::BLACK : chess::Board::WHITE)) {
        score = 8000; // Check bonus
    }
    b.undoMove(m, tmpState);
}
```

**Benefici**: Migliora tattiche senza overhead eccessivo (solo 8 mosse controllate)

---

#### 2. **Fix MVV-LVA con Victim-Only Ordering**
```cpp
inline constexpr int64_t MVV_TABLE[7] = {
    0,           // EMPTY
    1000,        // PAWN
    3200,        // KNIGHT
    3300,        // BISHOP
    5000,        // ROOK
    9000,        // QUEEN
    0            // KING (non dovrebbe essere catturato)
};

void Engine::addMVVLVABonus(const chess::Board::Move& m, const chess::Board& b, int64_t& score) noexcept {
    const uint8_t toPieceType = b.get(m.to) & chess::Board::MASK_PIECE_TYPE;
    if (toPieceType != chess::Board::EMPTY) {
        score += MVV_TABLE[toPieceType];
        return;
    }
    // En passant
    if (fromPieceType == chess::Board::PAWN) {
        if (chess::Board::fileOf(m.from.index) != chess::Board::fileOf(m.to.index)) {
            score += MVV_TABLE[chess::Board::PAWN];
        }
    }
}
```

---

#### 3. **Evitare Killer Duplicati**
Implementare il fix mostrato nel punto 2 dei problemi.

---

### **PRIORITÀ MEDIA** (Miglioramenti Tattici)

#### 4. **Counter-Move History**
```cpp
// In engine.hpp:
chess::Board::Move counterMoves[64][64]; // [prevFrom][prevTo] → bestResponse

// In updateKillerAndHistoryOnBetaCutoff:
if (prevMove.from.index != 64) { // Se esiste mossa precedente
    counterMoves[prevMove.from.index][prevMove.to.index] = m;
}

// In sortLegalMoves:
if (prevMove.from.index != 64) {
    const auto& counter = counterMoves[prevMove.from.index][prevMove.to.index];
    if (m.from.index == counter.from.index && m.to.index == counter.to.index) {
        score += 7500; // Tra killer e promotions
    }
}
```

**Benefici**: Migliora ordering in sequenze tattiche (es. dopo una cattura, risposta comune)

---

#### 5. **Capture History**
```cpp
// Separate history per captures (migliora ordering captures borderline)
int captureHistory[2][64][64]; // [color][to][capturedType]

// Aggiornare dopo ogni capture che causa cutoff
if (wasCapture) {
    const int bonus = depth * depth; // Bonus proporzionale a depth
    captureHistory[colorIdx][m.to.index][capturedType] += bonus;
}
```

---

### **PRIORITÀ BASSA** (Micro-ottimizzazioni)

#### 6. **SEE-based Ordering in Quiescence**
```cpp
// In quiescence, invece di solo MVV-LVA:
const int64_t see = staticExchangeEvaluation(b, m);
score = 10000 + see; // Ordina per SEE (migliori catture prima)
```

#### 7. **History Scaling Aumentato**
```cpp
// Aumentare clamp da 1000 a 2000
score = std::min(static_cast<int64_t>(2000), std::max(static_cast<int64_t>(0), histScore));
```

---

## 📈 TATTICHE NON IMPLEMENTATE

### 1. **Follow-up Moves (Continuation History)**
Stockfish usa: `history[prevMove1][prevMove2][currentMove]`  
**Benefici**: Cattura pattern tattici multi-mossa (es. scoperte, inchiodature)

### 2. **PV Move Ordering**
Salvare la Principal Variation completa e dare bonus alle mosse nel PV path  
**Benefici**: Nelle ricerche successive, PV moves esplorate per prime

### 3. **Capture Extensions in Move Ordering**
Dare bonus a captures che estendono forcing sequences (captures con recapture forzata)

### 4. **Threat Detection**
Rilevare mosse che minacciano material gain al prossimo ply  
**Esempio**: Se mossa scopre attacco alla regina avversaria → bonus alto

---

## 🎯 PIANO D'AZIONE RACCOMANDATO

### **FASE 1: Fixes Critici** (1-2 ore)
1. ✅ **Fix killer duplicates** (evita collisioni)
2. ✅ **Semplifica bad captures** (elimina 3-tier, usa -10000+SEE)
3. ✅ **MVV-only ordering** (rimuovi attacker penalty)

### **FASE 2: Tattiche Base** (2-3 ore)
4. ⚠️ **Lazy check detection** (top 8 mosse)
5. ⚠️ **Hash move validation** (controlla legalità)
6. ⚠️ **History scaling** (aumenta a 2000)

### **FASE 3: Tattiche Avanzate** (4-6 ore)
7. 🔮 **Counter-move history**
8. 🔮 **Capture history**
9. 🔮 **SEE-based quiescence ordering**

### **FASE 4: Testing & Tuning** (ongoing)
- Self-play tests con/senza ogni feature
- Perft & tactical test suites
- Lichess rating tracking

---

## 📊 METRICHE DI SUCCESSO

**Indicatori positivi**:
- ↓ Nodes searched (migliore pruning grazie a ordering)
- ↑ Beta cutoff rate (più cutoff al primo move)
- ↑ Rating Lichess (+50-100 Elo atteso con tutti i fix)
- ↑ Tactical test suite pass rate

**Monitorare**:
- Tempo medio per mossa (non deve aumentare >10%)
- TT hit rate (migliore ordering → migliore TT utilization)
- Quiescence depth media (migliore ordering → meno extend)

---

## 🔍 CONCLUSIONI

Il sistema di move ordering **funziona** ma ha **margini significativi di miglioramento**:

✅ **Punti di forza**:
- Hash move priorità massima (corretto)
- SEE-based good/bad capture separation (buono)
- Killer moves + history implementati

⚠️ **Punti deboli principali**:
1. **Check detection completamente disabilitato** (CRITICO per tattiche)
2. **MVV-LVA penalizza pezzi forti** (bug logico)
3. **Killer duplicates non gestiti** (spreco risorse)
4. **History troppo limitato** (range, no counter-moves)

🚀 **Implementando FASE 1+2** (fixes + tattiche base), stimo guadagno di **+80-120 Elo**.

---

**Fine Analisi**
