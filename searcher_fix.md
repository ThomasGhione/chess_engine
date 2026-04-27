Ran terminal command: sed -n '410,435p' engine/search/move_generator.cpp

Ran terminal command: cat -n engine/search/move_generator.cpp | grep -A 15 -B 5 "generateTacticalMoves"

Ran terminal command: sed -n '323,353p' engine/search/move_generator.cpp

Ran terminal command: wc -l engine/search/searcher.cpp engine/search/move_generator.cpp

Ran terminal command: sed -n '155,180p' board/board.cpp

Ran terminal command: grep -n -C 5 "inCheck(" board/board.cpp

Ottimo lavoro per aver rimosso il blocco da `sorter.cpp` e implementato il conteggio $O(1)$! I risultati del testing confermano che manteniamo esattamente lo stesso albero di ricerca guadagnando cicli CPU gratuiti, che è il caso ideale nel rifattorizzare un engine.

Ho analizzato a fondo la gestione della Search e della generazione in `engine/search/searcher.cpp` e `move_generator.cpp`. Attualmente, la ricerca contiene **bottleneck architetturali macroscopici** tipici dei motori di media forza. Spostare questi pattern verso approcci "lazy" o incrementali raddoppierà (o più) l'NPS (Nodes Per Second).

Ecco l'analisi dei bottleneck suddivisi per livello di criticità logica:

### 🔴 Livello Criticità: MASSIMA (Impatto drastico su NPS)

#### 1. "Eager" Strict Legal Move Generation (Il MovePicker è finto-Lazy)
Attualmente in `Searcher::search` e in `QSearch` facciamo questo:
```cpp
// Viene chiamato PRIMA di estrarre la prima mossa
MoveList<chess::Board::Move> moves = engine::MoveGenerator::generateLegalMoves(b, ...);
Sorter::MovePickerData movePicker = Sorter::sortLegalMoves(moves, ...);
```
**Perché è un collo di bottiglia?** 
L'engine valuta rigorsamente la legalità (raggi della inchiodatura, maschere di `isLegalPseudoMove`, ecc.) per **tutte** le 30-40 mosse pseudo-legali della posizione. Tuttavia, la ricerca Alpha-Beta ha la _Hash Move_ e le _Killer Moves_! Nel 90% dei nodi con *beta-cutoff*, basta cercare $1$ o $2$ mosse. Calcolare la legalità assoluta di tutte le mosse restanti (che non verranno mai visitate) macina CPU inutilmente.
**Soluzione Target:** Il MovePicker deve generare _pseudo-legal_ a scaglioni (Fase 1: Hash; Fase 2: Catture pseudo-legali; Fase 3: Quiete pseudo-legali) ed è il loop PVS che fa `if (!b.isLegal(...)) continue;`.

#### 2. Ricalcolo pesante di `b.inCheck()` dinamicamente nel loop
Nel loop principale (`searcher.cpp:590`), ogni volta che serve `givesCheck` per la LMR o per i Check Extensions:
```cpp
const bool givesCheck = needsCheckInfo ? b.inCheck(oppColor) : false;
```
**Perché è un collo di bottiglia?** 
Per calcolare `inCheck`, la scacchiera scansiona gli attacchi dal Re avversario guardando Raggi, Cavalli e Pedoni al contrario. Farlo dopo il `doMoveWithPromotion` per _ogni iterazione_ candidata alla LMR in un nodo è devastante a profondità alte.
**Soluzione Target:** Implementare un pattern più leggero come un `b.givesCheck(move)` (che calcola le scoperte e gli attacchi diretti *prima* di fare la mossa usando i bitboard delle inchiodature aggiornati, evitando `inCheck()` totali) oppure tracciarlo nel `MoveState`.

---

### 🟠 Livello Criticità: ALTA (Dispendio RAM / Branching Tree non ottimoc)

#### 3. Evasions Complete nella Quiescence Search
In QSearch, nel blocco intercettivo per gestire i Re sotto scacco (Depth < MAX_QSEARCH_DEPTH, `searcher.cpp:907`), l'engine chiama:
```cpp
MoveList<chess::Board::Move> evasions = engine::MoveGenerator::generateQSearchEvasions(b);
// (che internamente lancia un full generateLegalMoves)
```
**Perché è un collo di bottiglia?** 
Le Evasions per uscire da scacco all'interno del ramo QSearch dovrebbero essere rigidamente potate e veloci da generare. Noi stiamo scatenando ancora una volta la generazione legale completa rigorosa per difese che includono spinte silenziose di pedoni o movimenti ininfluenti, intasando un albero che dovrebbe finire rapidissimamente.

#### 4. Doppio calcolo di Legalità in alcuni fallback
In parti come macro-step 4 dei ritorni (`searcher.cpp:265`), qualora si interrompesse l'iterative deepening, rigeneriamo le legal moves con `generateLegalMoves(board)`. Non è un bottleneck nel node loop ma blocca il memory allocator di sistema. `MoveList` su stack è passabile, ma le `std::array` per i *pin-rays* calcolate nei generatori costano.

---

### 🟡 Livello Criticità: MEDIA (Migliorare per Elo incrementale)

#### 5. Separazione netta doMove / undoMove frammentata
L'uso di `doMoveWithPromotion(b, m, state)` è presente in molteplici funzioni isolate anziché all'interno dell'unico loop del delegato della search. C'è logica di "undo/do" dispersa in QSearch e in MainSearch. Centralizzare la verifica di "scacco" o futility _prima_ del doMove salverebbe cicli di `__builtin_popcountll` sui bitboard di aggiornamento che sono altrimenti gettati via dopo 2 righe da `undoMove()`.

---

### Come procedere?
Ti propongo di aggredire il **Punto 1** come prima mossa: è il più complesso perché richiederà un refactoring del `MovePicker` in modo che lavori assorbendo i _generatori pseudo-legali_ a "fasi" (e non si prenda dal piatto un array gigante già vagliato e ripulito).
Se invece preferisci un passo più isolato (ma succoso), possiamo uccidere il **Punto 2** (aggirando `inCheck()` dalla pre-mossa).

Quale macro-fase vogliamo sbloccare prima?