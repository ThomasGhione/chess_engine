# Evaluation Function - Code Review Completa

**Data:** 9 Febbraio 2026  
**Reviewer:** Code Review Esperto  
**Obiettivo:** Identificare bug, errori logici, valori fuorvianti e problemi architetturali nella funzione di valutazione che impediscono all'engine di superare ~1600 ELO nonostante tutte le strategie necessarie per >2000 ELO siano implementate.

**File analizzati:**
- `engine/evaluate.cpp` (1504 righe)
- `engine/basebonuspenaltyvalues.hpp`
- `engine/piecevaluetables.hpp`
- `engine/engine.hpp`
- `engine/search.cpp` (1414 righe)

---

## Sommario Esecutivo

L'engine ha tutte le componenti giuste (PSQT, mobilità, struttura pedoni, sicurezza del re, ecc.), ma la valutazione soffre di **problemi strutturali gravi** che la rendono inaffidabile:

1. **Doppio conteggio massiccio** - Gli stessi concetti sono valutati 2-3 volte con segni diversi
2. **Material contempt distruttivo** - Raddoppia il costo del materiale, distorcendo completamente la valutazione
3. **Assenza di tapered evaluation** - Transizioni brusche tra fasi causano discontinuità
4. **Valori dei bonus/penalty non calibrati** - Molti valori sono stati ridotti/aumentati ad-hoc senza coerenza
5. **Bug logici nella struttura pedoni** - Backward pawn e pawn chain con errori di segno
6. **evalPassiveRooks() penalizza TUTTE le torri** - Bug gravissimo
7. **evalEarlyQueen() troppo aggressiva** - Penalità -200cp per qualsiasi mossa di donna
8. **Game phase basata su fullMoveClock** - Errata: usa il numero di mosse, non lo stato della posizione

---

## Sezione 1: BUG CRITICI (Impact Diretto su ELO)

### BUG 1.1: Material Contempt Raddoppia il Costo del Materiale ⚠️ CRITICO

**File:** `evaluate.cpp`, righe 1474-1498  
**Gravità:** ★★★★★

Il "material contempt" alla fine di `evaluate()` aggiunge una penalità basata su `getMaterialDelta()`, ma `getMaterialDelta()` è GIÀ il primo componente di `eval`. Questo effettivamente **raddoppia o triplica** il valore del materiale.

```cpp
int64_t eval = getMaterialDelta(board);  // ← materiale già contato
// ... tutti gli altri bonus/penalty ...

// Alla fine:
const int64_t matDelta = getMaterialDelta(board);  // ← ricalcolato!
if (absMatDelta > 50) {
    // Per perdite >= 300cp: penalty = 100% del delta
    contemptPenalty = absMatDelta;  // ← RADDOPPIA il materiale!
    eval += (matDelta > 0) ? contemptPenalty : -contemptPenalty;
}
```

**Effetto pratico:** Se il bianco ha un pedone in più (+100cp di materiale), l'eval diventa:
- Materiale: +100
- Contempt: +50 (50% per < 300cp)
- Totale materiale effettivo: +150 anziché +100

Se perde una donna (-900cp):
- Materiale: -900
- Contempt: -900 (100% per >= 300cp)  
- Totale materiale effettivo: **-1800** anziché -900

**Conseguenza:** L'engine diventa PARALIZZATO. Qualsiasi sacrificio anche tatticamente corretto viene punito con il doppio del suo costo. L'engine non sacrificherà MAI un pedone per un attacco al re, non scambierà pezzi in posizioni vantaggiose, e rifiuterà gambetti standard.

**Fix:** Rimuovere completamente il material contempt. La search con alpha-beta già impedisce sacrifici speculativi. Se si vuole un contempt, usare un valore FISSO molto piccolo (5-10cp) nella search, non nella eval.

---

### BUG 1.2: evalPassiveRooks() Penalizza TUTTE le Torri ⚠️ CRITICO

**File:** `evaluate.cpp`, righe 372-394  
**Gravità:** ★★★★★

Questa funzione applica **tre penalità** a quasi tutte le torri sulla scacchiera:

```cpp
// Low mobility
if (mobility <= 3) score += sign * (-25);

// Rook blocked by own pawn on the same file
if (ownPawns & FILE_MASKS[file]) score += sign * (-15);

// Not on 7th rank penalty
if ((side == 0 && rank != 6) || (side == 1 && rank != 1)) {
    score += sign * (-10);  // TUTTE le torri non sulla 7a!!!
}
```

**Problema 1:** La penalità "not on 7th rank" si applica a OGNI torre che non è sulla 7a traversa. All'inizio della partita, entrambe le torri di ciascun giocatore ricevono -10cp ciascuna = -20cp per lato, netto 0 ma aggiunge rumore alla valutazione.

**Problema 2:** "Rook blocked by own pawn on same file" si applica a TUTTE le torri che hanno un pedone amico sulla stessa colonna, anche se la torre è davanti al pedone (non bloccata).

**Problema 3:** Mobilità ≤ 3 è molto comune per torri in apertura/mediogioco. Ogni torre con mobilità ≤ 3 riceve -25cp, il che si somma alle penalità di `evalTrappedPieces()` creando **doppio conteggio**.

**Fix:** Rimuovere `evalPassiveRooks()` o riscriverla da zero. Le penalità per torri passive sono già coperte da `evalTrappedPieces()` (mobilità) e `evalRooks()` (colonne aperte/semiaperte).

---

### BUG 1.3: evalEarlyQueen() Penalità Eccessiva (-200cp) ⚠️ CRITICO

**File:** `evaluate.cpp`, righe 587-598  
**Gravità:** ★★★★

```cpp
int64_t Engine::evalEarlyQueen(const chess::Board& b) noexcept {
    // White queen
    if (b.queens_bb[0] && !(b.queens_bb[0] & chess::Board::bitMask(59))) {
        score += ATTACKED_QUEEN_PENALTY * 8;  // -25 * 8 = -200!!!
    }
}
```

`ATTACKED_QUEEN_PENALTY` è -25, moltiplicato per 8 = **-200cp** per qualsiasi donna non sulla sua casella iniziale. Questo significa che dopo **qualsiasi mossa** della donna (anche Qd2, Qe2, che sono mosse normali), il bianco riceve -200cp. Equivale a dire che muovere la donna costa 2 pedoni.

**Conseguenza:** L'engine non muoverà MAI la donna in apertura, nemmeno per catturare un pezzo appeso o difendere. Se l'avversario attacca con la donna, l'engine non potrà contrattaccare.

**Fix:** Ridurre a ~-20cp e applicare solo nelle prime 6-8 mosse. In alternativa, penalizzare solo se i pezzi minori non sono sviluppati.

---

### BUG 1.4: Backward Pawn - Boundary Check Mancante

**File:** `evaluate.cpp`, righe 186-194 e 253-264  
**Gravità:** ★★★

```cpp
// White backward pawn:
const bool hasSupport = ((whitePawns & chess::Board::bitMask(sq + 7)) != 0) ||
                       ((whitePawns & chess::Board::bitMask(sq + 9)) != 0);
```

`sq + 7` e `sq + 9` possono causare **wrapping** tra file. Se un pedone bianco è in a3 (sq=40), `sq+7=47` è h4 e `sq+9=49` è b5. Ma per un pedone in h3 (sq=47), `sq+9=56` è a1! Non viene controllato se il file è 0 o 7 prima di accedere alle diagonali.

Lo stesso problema esiste per i pedoni neri con `sq-7` e `sq-9`.

**Fix:** Aggiungere controlli sui file come fatto in pawn chain:
```cpp
const bool hasSupport = (file > 0 && (whitePawns & chess::Board::bitMask(sq + 7))) ||
                       (file < 7 && (whitePawns & chess::Board::bitMask(sq + 9)));
```

---

### BUG 1.5: evalBadKingPosition() Troppo Aggressiva

**File:** `evaluate.cpp`, righe 828-842  
**Gravità:** ★★★

```cpp
// White re sopra 2a traversa o Black sotto 7a
if ((side == 0 && rank < 6) || (side == 1 && rank > 1)) {
    score += sign * KING_EXPOSED_PENALTY;  // -25cp
}
```

Questa penalità si applica al re bianco se non è sulle traverse 1-2 (rank 6-7 nella convenzione). Ma in ENDGAME il re DEVE essere attivo e andare al centro! Questa funzione viene chiamata in `isEarlyMiddlegame` e `isMiddlegame`, ma:

1. Il re dopo l'arrocco corto è su g1 (rank=7, file=6) → OK
2. Ma un re su e1 che non ha arroccato (rank=7) → nessuna penalità, eppure è esposto al centro!
3. In mediogioco, se il re si muove anche di una casella dalla prima traversa → -25cp

**Fix:** Questa funzione è ridondante con `evalKingSafety()` e `evalCastlingBonus()`. Rimuoverla o limitarla ai casi dove il re è veramente fuori posto (sopra la 4a traversa in mediogioco).

---

## Sezione 2: ERRORI LOGICI E DOPPIO CONTEGGIO

### 2.1: Doppio Conteggio Mobilità/Pezzi Intrappolati

**Gravità:** ★★★★

`evalTrappedPieces()` e `computeAttackData()` calcolano ENTRAMBI la mobilità dei pezzi indipendentemente:

- `computeAttackData()` calcola `knightMobility`, `bishopMobility`, ecc. per `evalMobility()`
- `evalTrappedPieces()` ricalcola la mobilità per OGNI pezzo con `__builtin_popcountll(attacks & ~occ)`

Risultato: un cavallo con mobilità 2 riceve:
1. Penalità `LOW_MOBILITY_KNIGHT_PENALTY` = -10 da `evalTrappedPieces()`
2. Mobilità bassa in `evalMobility()` (contributo negativo tramite `knightMobility`)

La penalità è contata DUE VOLTE.

**Fix:** Rimuovere `evalTrappedPieces()` e usare solo i dati di `AttackData` per la mobilità. Oppure rendere `evalTrappedPieces()` un wrapper che usa `AttackData` anziché ricalcolare tutto.

---

### 2.2: Doppio Conteggio Cavallo sul Bordo

**Gravità:** ★★★

`evalKnightOnRim()` penalizza cavalli su file A/H (-30), B/G (-15), e traverse 1/8 (-10).

Ma le PSQT (`KNIGHT_VALUES_TABLE`) già penalizzano pesantemente i cavalli sul bordo:
- Angoli: -50
- Bordi: -40 e -30
- Traverse 1/8: -50 e -40

Risultato: un cavallo in a1 riceve:
1. PSQT: -50
2. evalKnightOnRim: -30 (file A) + -10 (rank 1) = -40

Totale: -90cp! Troppo aggressivo.

**Fix:** Rimuovere `evalKnightOnRim()`. Le PSQT coprono già questo concetto in modo più granulare.

---

### 2.3: Doppio Conteggio Arrocco

**Gravità:** ★★★

L'arrocco è valutato in TRE posti diversi:
1. `evalCastlingBonus()` → +35 se arroccato, -60 se ha perso il diritto
2. `evalEarlyKing()` → -15 se il re non è su e1/g1
3. `evalKingSafety()` → -20 (`KING_NON_CASTLING_PENALTY`) se non ha arroccato + penalità pedoni mossi

Un giocatore che non ha arroccato riceve cumulativamente:
- evalCastlingBonus: 0 o -60
- evalEarlyKing: -15
- evalKingSafety: -20 + penalità pedoni

Totale: fino a **-95cp** per non aver arroccato. Troppo!

**Fix:** Unificare la logica di arrocco in una singola funzione `evalCastling()`.

---

### 2.4: Doppio Conteggio Torre su Colonna Aperta

**Gravità:** ★★

`evalRooks()` dà bonus per colonne aperte/semiaperte.
`evalPassiveRooks()` dà penalità se un pedone proprio è sulla stessa colonna.

Questi sono lo stesso concetto valutato due volte con segni opposti.

---

### 2.5: evalCoordination + evalTrappedPieces + evalMobility

**Gravità:** ★★★

Un pezzo isolato viene penalizzato da:
1. `evalPieceCoordination()`: -12cp se nessun pezzo amico entro distanza Manhattan ≤ 2
2. `evalTrappedPieces()`: penalità se mobilità ≤ 3
3. `evalMobility()`: mobilità bassa riduce lo score

Tre penalità diverse per lo stesso problema (pezzo mal piazzato).

---

## Sezione 3: GAME PHASE E TAPERED EVALUATION

### 3.1: Game Phase Basata su fullMoveClock ⚠️ CRITICO

**Gravità:** ★★★★★

```cpp
const bool isOpening = !isEndgame && (fullMoves < OPENING_MOVES);  // < 10 mosse
const bool isEarlyMiddlegame = !isEndgame && !isOpening && (fullMoves < EARLY_MG_MOVES);  // < 15
```

Usare il numero di mosse per determinare la fase è **fondamentalmente errato**. Una partita può essere in endgame alla mossa 15 (se c'è stato uno scambio massiccio) o ancora in apertura alla mossa 15 (se si gioca una difesa chiusa lenta).

**Conseguenza:** 
- Alla mossa 10, l'engine smette di sviluppare i pezzi (esce dall'apertura)
- Alla mossa 15, smette di valutare lo sviluppo
- In una partita rapida con molti scambi, può essere in "middlegame" quando è già in endgame puro

**Fix:** Usare una **tapered evaluation** basata sul materiale (standard industriale):

```
gamePhase = N_cavalieri*1 + N_alfieri*1 + N_torri*2 + N_donne*4
totalPhase = 24 (max all pieces)
mgWeight = gamePhase
egWeight = totalPhase - gamePhase

eval = (mgEval * mgWeight + egEval * egWeight) / totalPhase
```

Questo è il metodo usato da Stockfish, Fruit, e virtualmente ogni engine >2000 ELO.

---

### 3.2: Transizione Brusca tra Fasi (No Tapered Eval)

**Gravità:** ★★★★★

Attualmente, la valutazione ha un `if/else if/else if/else` che seleziona UNA fase. Quando si passa da una fase all'altra (es. da "opening" a "early middlegame"), i bonus/penalty cambiano BRUSCAMENTE.

Esempio: alla mossa 9, l'engine valuta `evalMinorPieceDevelopment()` con bonus +15 per pezzo sviluppato. Alla mossa 10, questo bonus SCOMPARE completamente perché si entra in "early middlegame". Questo causa un **salto nella valutazione** che confonde la search.

In una tapered eval, il bonus per lo sviluppo passerebbe gradualmente da 15 a 0 man mano che i pezzi vengono scambiati.

---

### 3.3: Fasi Non Selezionano le Funzioni Giuste

**Gravità:** ★★★

- **Opening:** non valuta `evalTrappedPieces()` → l'engine può non accorgersi di pezzi intrappolati
- **Opening:** non valuta `evalKingSafety()` → ignora re esposti nelle aperture aggressive
- **Opening:** non valuta `evalRooks()` → ignora la colonna aperta dopo 1.e4 e5 2.d4 exd4
- **Middlegame:** non valuta `evalMinorPieceDevelopment()` → alla mossa 16 ignora pezzi non sviluppati
- **Endgame:** non valuta `evalCentralControl()` → il controllo del centro è importante anche in endgame
- **Endgame:** non valuta `evalPassiveRooks()` → le torri passive in endgame sono un problema grave

---

## Sezione 4: VALORI DEI BONUS/PENALTY NON CALIBRATI

### 4.1: Tabella Riepilogativa dei Valori Problematici

| Parametro | Valore Attuale | Valore Tipico (engine >2000) | Note |
|---|---|---|---|
| `PASSED_PAWN_BONUS` | 25 | 20-60 (scalato per rank) | OK come base, ma l'avanzamento bonus è troppo basso |
| `ISOLATED_PAWN_PENALTY` | -18 | -10 to -15 | Leggermente troppo alto |
| `DOUBLED_PAWN_PENALTY` | -20 | -10 to -15 | Troppo alto, pedoni doppiati non sono così gravi |
| `CENTER_CONTROL_BONUS` | 15 | 10-15 | OK |
| `HANGING_MINOR_PENALTY` | -80 | -40 to -60 | Un po' alto, la SEE già valuta |
| `HANGING_QUEEN_PENALTY` | -200 | -100 to -150 | Alto, la SEE e la search già proteggono |
| `KING_NON_CASTLING_PENALTY` | 20 | 10-15 | Troppo alto, soprattutto cumulato con gli altri |
| `CASTLING_BONUS` | 35 | 20-30 | Leggermente alto |
| `DEVELOPMENT_BONUS` | 15 | 8-12 | Troppo alto per pezzo (4 pezzi = 60cp!) |
| `EARLY_ROOK_PENALTY` | -30 | -10 to -15 | Troppo aggressivo |
| `BISHOP_PAIR_BONUS` | 30 | 30-50 | OK ma non lo vedo usato nella eval! |
| `OUTPOST_KNIGHT_BONUS` | 30 | 15-25 | Leggermente alto |

### 4.2: BISHOP_PAIR_BONUS Mai Usato!

**Gravità:** ★★★★

Il bonus per la coppia di alfieri (`BISHOP_PAIR_BONUS = 30`) è DEFINITO ma non viene MAI utilizzato in `evaluate()`. Nessuna funzione lo chiama. Questo è un bonus fondamentale che distingue engine 1600 da engine 2000+.

**Fix:** Aggiungere nella valutazione:
```cpp
// Bishop pair bonus
const int whiteBishops = __builtin_popcountll(board.bishops_bb[0]);
const int blackBishops = __builtin_popcountll(board.bishops_bb[1]);
if (whiteBishops >= 2) eval += BISHOP_PAIR_BONUS;
if (blackBishops >= 2) eval -= BISHOP_PAIR_BONUS;
```

---

### 4.3: DEVELOPMENT_BONUS Troppo Alto

**Gravità:** ★★★

`DEVELOPMENT_BONUS = 15` per pezzo sviluppato. Con 4 pezzi minori (2 cavalieri + 2 alfieri), il bonus massimo è 60cp (più di mezzo pedone). Questo può far preferire mosse di sviluppo insensate piuttosto che catturare materiale.

**Fix:** Ridurre a 8-10cp per pezzo.

---

### 4.4: EARLY_ROOK_PENALTY Fallace

**Gravità:** ★★★

```cpp
if (b.rooks_bb[0] && !(b.rooks_bb[0] & chess::Board::bitMask(56)) && !(b.rooks_bb[0] & chess::Board::bitMask(63))) {
    score += EARLY_ROOK_PENALTY; // -30cp
}
```

Se NESSUNA delle due torri bianche è su a1 o h1, applica -30cp. Ma dopo l'arrocco, la torre si muove su f1 (posizione 61) → la torre non è né su 56 né su 63 → penalità -30cp applicata ANCHE DOPO l'arrocco!

**Fix:** Questa funzione è concettualmente sbagliata. Non è possibile determinare se una torre è stata mossa "troppo presto" solo dalla sua posizione. Rimuoverla.

---

## Sezione 5: PAWN STRUCTURE BUGS

### 5.1: Passed Pawn Advancement Asimmetrico

**Gravità:** ★★★

Per i pedoni passati bianchi:
```cpp
const int advancement = 6 - rank;  // rank 6→0, rank 1→5
score += advancement * (isEndgame ? 6 : 2);
// Extra danger on 7th rank
if (rank == 1) score += isEndgame ? 40 : 20;
// Endgame extra
score += (6 - rank) * 4;
```

Per i pedoni passati neri:
```cpp
const int advancement = rank;  // rank 1→1, rank 6→6
score -= advancement * (isEndgame ? 6 : 2);
// Extra danger on 7th rank
if (rank == 6) score -= isEndgame ? 40 : 20;
// Endgame extra
score -= rank * 4;
```

**Problema:** Il pedone bianco su rank 1 (traversa 7) riceve:
- advancement: (6-1) * 6 = 30 in endgame
- Extra 7th rank: 40
- Endgame extra: (6-1) * 4 = 20
- Totale: 25 + 30 + 40 + 20 = **115cp**

Il pedone nero su rank 6 (traversa 7) riceve:
- advancement: 6 * 6 = 36 in endgame
- Extra 7th rank: 40
- Endgame extra: 6 * 4 = 24
- Totale: 25 + 36 + 40 + 24 = **125cp**

C'è un'**asimmetria di 10cp** nel bonus per passed pawn tra bianco e nero. Questo è un bug sottile.

**Problema 2:** Il bonus endgame extra `(6 - rank) * 4` viene aggiunto DENTRO al blocco `if passed pawn`, ma FUORI da un `else`, quindi si aggiunge SEMPRE ai pedoni passati in endgame, duplicando il bonus di avanzamento.

**Fix:** Unificare la formula di avanzamento per entrambi i colori:
```
advancement_from_start = (for white) 6 - rank; (for black) rank - 1;
// 0 = start, 5 = one step from promotion
bonus = PASSED_PAWN_BASE + advancement * advancement * scale_factor;
```

Usare una formula **quadratica** per l'avanzamento (standard negli engine forti): un pedone passato sulla 7a vale molto più del doppio di uno sulla 4a.

---

### 5.2: Passed Pawn Blocked Riduzione Troppo Debole

**Gravità:** ★★

Se un pedone passato è bloccato da un pedone nemico direttamente davanti:
```cpp
score -= PASSED_PAWN_BONUS / 2;  // Solo -12cp
```

Un pedone passato bloccato ha un valore molto inferiore. La riduzione dovrebbe essere almeno del 60-80%.

---

### 5.3: Pawn Chain Bonus Hardcoded a 15cp

**Gravità:** ★★

Il bonus per pawn chain è hardcoded a `15` anziché usare una costante da `basebonuspenaltyvalues.hpp`. Questo rende impossibile il tuning.

---

## Sezione 6: KING SAFETY BUGS

### 6.1: evalKingSafety() Asimmetrica

**Gravità:** ★★★

```cpp
// White king shield:
score += sign * __builtin_popcountll(whitePawns & shieldSquares) * CASTLE_PAWN_SUPPORT_BONUS;  // 4cp

// Black king shield:
score += sign * __builtin_popcountll(blackPawns & shieldSquares) * 10;  // 10cp!!!
```

Il bonus per lo scudo pedonale del re è **4cp per il bianco** e **10cp per il nero**! Questo rende il nero artificialmente più sicuro dopo l'arrocco.

**Fix:** Usare `CASTLE_PAWN_SUPPORT_BONUS` per entrambi i lati, e impostarlo a un valore ragionevole (7-10cp).

---

### 6.2: evalKingSafety() Penalità Pre-Arrocco Troppo Forte

**Gravità:** ★★

La penalità per pedoni mossi prima dell'arrocco (6cp per pedone) si applica a TUTTI i pedoni sul lato dell'arrocco. Ma muovere g2-g3 per preparare l'arrocco in fianchetto è POSITIVO, non negativo!

---

## Sezione 7: ENDGAME EVALUATION

### 7.1: evalRookEndgamePressure() / evalQueenEndgamePressure() Valori Enormi

**Gravità:** ★★★

```cpp
// Queen endgame:
score += sign * edgeProximity * QUEEN_EG_EDGE_BONUS;  // 120 * 7 = 840cp!!!
score += sign * proximityBonus * 20;  // Fino a 280cp
score += sign * 40;  // Bonus posizione donna
// TOTALE POSSIBILE: >1000cp
```

Il bonus per spingere il re nemico al bordo in endgame può raggiungere **oltre 1000cp** (10 pedoni!). Questo distorce completamente la valutazione e può far preferire posizioni con il re al bordo ma senza materiale.

**Fix:** Limitare i bonus a un range ragionevole (100-200cp totali).

---

### 7.2: evalEndgameKingActivity() Direzione Invertita

**Gravità:** ★★★

```cpp
score += (side == 0 ? -best : best) * 10;
```

Per il bianco, aggiunge `-best * 10`. `best` è la distanza Manhattan dal centro (più piccola = più vicino al centro). Quindi un re bianco vicino al centro (best=1) riceve **-10cp**, e uno lontano (best=4) riceve **-40cp**.

Questo significa che il re bianco è PENALIZZATO per essere al centro e PREMIATO per stare lontano. **L'effetto è invertito!** Un re che si avvicina al centro dovrebbe ricevere un bonus positivo.

**Fix:** Invertire il segno:
```cpp
score += (side == 0 ? (7 - best) : -(7 - best)) * 10;
```

---

### 7.3: PIECE_ENDGAME_THRESHOLD Troppo Alto

**Gravità:** ★★★

```cpp
constexpr int PIECE_ENDGAME_THRESHOLD = 8;  // <= 8 pezzi maggiori = endgame
```

8 pezzi maggiori significa 4 pezzi per lato (es. cavallo + alfiere + 2 torri). Questa è ancora una posizione di mediogioco! Il threshold dovrebbe essere 4-6.

Effetto: troppe posizioni sono classificate come "endgame" quando sono ancora mediogioco.

---

## Sezione 8: ARCHITETTURA E DESIGN

### 8.1: Assenza di Tapered Evaluation (Problema Più Grave)

**Gravità:** ★★★★★

L'approccio if/else per le fasi è il singolo problema architetturale più grave. Ogni engine moderno >1800 ELO usa tapered evaluation.

**Come funziona (Stockfish/Fruit):**

```
Per ogni termine di valutazione:
  1. Calcola mg_score (middlegame value)
  2. Calcola eg_score (endgame value)

phase = compute_phase(board)  // 0=endgame, 256=opening

final_eval = (mg_score * phase + eg_score * (256 - phase)) / 256
```

Questo:
- Elimina le discontinuità tra fasi
- Permette ad ogni termine di avere valori diversi in MG e EG
- Rende il tuning molto più facile
- È lo standard usato da tutti gli engine >2000 ELO

### 8.2: Troppi Termini di Valutazione

**Gravità:** ★★★

L'engine ha ~25 funzioni di valutazione diverse. Engine forti tipicamente ne hanno 8-12, ben calibrati. Ogni termine aggiuntivo:
1. Aggiunge rumore se non calibrato
2. Rallenta la valutazione (meno nodi/secondo)
3. Aumenta la probabilità di doppio conteggio
4. Rende il tuning impossibile

**Fix:** Consolidare in termini fondamentali:
1. Materiale + PSQT (tapered)
2. Mobilità
3. Struttura pedoni (passed, isolated, doubled)
4. Sicurezza del re
5. Torri su colonne aperte/7a traversa
6. Bishop pair
7. Hanging pieces (solo con attack data)

---

## Sezione 9: PIANO DI INTERVENTO

### Fase 1: Fix Immediati (Bug Critici) — Priorità MASSIMA

| # | Azione | Impatto Stimato | Rischio |
|---|--------|-----------------|---------|
| 1 | **Rimuovere material contempt** (righe 1460-1498) | +50-100 ELO | Basso |
| 2 | **Rimuovere `evalPassiveRooks()`** (doppio conteggio) | +20-40 ELO | Basso |
| 3 | **Ridurre `evalEarlyQueen()` da -200cp a -20cp** | +20-30 ELO | Basso |
| 4 | **Fix asimmetria pawn shield** (4cp vs 10cp) | +10-20 ELO | Basso |
| 5 | **Aggiungere `BISHOP_PAIR_BONUS`** nella eval | +15-25 ELO | Basso |
| 6 | **Fix `evalEndgameKingActivity()` segno invertito** | +10-20 ELO | Basso |
| 7 | **Fix backward pawn boundary check** | +5 ELO | Basso |

### Fase 2: Rimozione Doppio Conteggio — Priorità ALTA

| # | Azione | Impatto Stimato | Rischio |
|---|--------|-----------------|---------|
| 8 | **Rimuovere `evalKnightOnRim()`** (coperto da PSQT) | +10-15 ELO | Basso |
| 9 | **Unificare mobilità** (rimuovere `evalTrappedPieces`, usare AttackData) | +10-20 ELO | Medio |
| 10 | **Unificare arrocco** (merge evalCastlingBonus + evalEarlyKing + parte evalKingSafety) | +10-15 ELO | Medio |
| 11 | **Rimuovere `evalEarlyKing()`** (coperto da evalCastling) | +5 ELO | Basso |
| 12 | **Rimuovere `evalEarlyRook()`** (logica fallace) | +5 ELO | Basso |
| 13 | **Rimuovere `evalBadKingPosition()`** (doppio con king safety) | +5 ELO | Basso |

### Fase 3: Tapered Evaluation — Priorità ALTA (Più Grande Miglioramento)

| # | Azione | Impatto Stimato | Rischio |
|---|--------|-----------------|---------|
| 14 | **Implementare tapered eval** con phase basata su materiale | +100-200 ELO | Alto |
| 15 | **Rimuovere if/else fasi** e applicare TUTTI i termini con peso mg/eg | — (parte di 14) | — |
| 16 | **PSQT separate per MG e EG** (già fatto per pedoni e re) | — (parte di 14) | — |

### Fase 4: Tuning dei Valori — Priorità MEDIA

| # | Azione | Impatto Stimato | Rischio |
|---|--------|-----------------|---------|
| 17 | **Ridurre `DEVELOPMENT_BONUS`** da 15 a 8-10 | +5-10 ELO | Basso |
| 18 | **Ridurre `DOUBLED_PAWN_PENALTY`** da -20 a -12 | +5 ELO | Basso |
| 19 | **Ridurre `HANGING_*` penalties** del 20-30% | +10-15 ELO | Basso |
| 20 | **Ridurre `KING_NON_CASTLING_PENALTY`** da 20 a 10 | +5 ELO | Basso |
| 21 | **Ridurre `PIECE_ENDGAME_THRESHOLD`** da 8 a 5 | +10-15 ELO | Basso |
| 22 | **Bonus passed pawn quadratico** | +10-15 ELO | Medio |

### Fase 5: Consolidamento — Priorità BASSA

| # | Azione | Impatto Stimato | Rischio |
|---|--------|-----------------|---------|
| 23 | Rimuovere `evalBlockedCenterWithPieces()` (troppo specifico, rumore) | +0-5 ELO | Basso |
| 24 | Rimuovere `evalBlockedPawnByBishops()` (niche, poco impatto) | +0-5 ELO | Basso |
| 25 | Considerare Texel tuning automatico dei parametri | +50-100 ELO | Alto |

---

## Sezione 10: NOTE SULLA SEARCH

Anche se il focus di questa review è l'evaluation, ho notato questi problemi nella search che interagiscono con la eval:

### 10.1: Quiescence Search Chiama evaluate() a Ogni Nodo

`quiescenceSearch()` chiama `this->evaluate(b)` come stand-pat. Con 25+ funzioni di valutazione e magic bitboard lookups, questo è MOLTO costoso. La qsearch può esplorare migliaia di nodi e ogni evaluate() completa è pesante.

**Fix a lungo termine:** Usare incremental evaluation (aggiornare eval dopo ogni mossa) o lazy evaluation più aggressiva.

### 10.2: Null Move Pruning Disabilitato

Il commento dice "Null Move Pruning è disabilitato". NMP è uno dei pruning più importanti, può raddoppiare la velocità della search. La sua assenza costa ~100-200 ELO da sola.

### 10.3: Nessuna LMP / Futility Pruning nella Search Principale

La search non ha Late Move Pruning né futility pruning nella search principale (solo LMR). Questo limita la profondità raggiungibile.

---

## Impatto Cumulativo Stimato

| Fase | ELO Stimato |
|------|-------------|
| Base attuale | ~1600 |
| Dopo Fase 1 (bug critici) | ~1750-1800 |
| Dopo Fase 2 (doppio conteggio) | ~1850-1900 |
| Dopo Fase 3 (tapered eval) | ~2000-2100 |
| Dopo Fase 4 (tuning) | ~2100-2150 |
| Dopo Fase 5 + NMP | ~2200+ |

---

## Appendice: Quick Reference per Fix Immediati

### Ordine consigliato di implementazione:
1. Rimuovere material contempt (5 minuti, impatto enorme)
2. Fix evalEarlyQueen (2 minuti)
3. Rimuovere evalPassiveRooks (2 minuti)
4. Aggiungere bishop pair bonus (5 minuti)
5. Fix asimmetria king shield (2 minuti)
6. Fix evalEndgameKingActivity segno (2 minuti)
7. Ridurre PIECE_ENDGAME_THRESHOLD a 5 (1 minuto)

Totale Fase 1: ~20 minuti di lavoro, stimati +100-200 ELO.
