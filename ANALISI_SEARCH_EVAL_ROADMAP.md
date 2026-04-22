# Analisi Search + Evaluation (Roadmap)

## 1) Stato attuale: base gia solida
Il motore ha gia molte tecniche importanti:

- Iterative deepening + aspiration windows
- PVS (root e nodi interni)
- TT con hash move
- Move ordering con history, killer, counter-move, capture history, SEE
- LMR, LMP, futility pruning, reverse futility pruning
- Null move pruning (con verification search ad alta profondita)
- Mate distance pruning
- Quiescence search con delta pruning + SEE pruning

Quindi il lavoro da fare non e "aggiungere tutto da zero", ma chiudere gap ad alto impatto Elo.

## 2) Criticita da correggere subito (alta priorita)

[V]
### 2.1 QSearch: ritorno bound errato in delta pruning dinamico
- Nel ramo di delta pruning dinamico in qsearch, in caso di prune viene ritornato `cutoffValue(alpha, beta, usIsWhite)`.
- Questo e rischioso perche il prune e concettualmente fail-low, non fail-high.
- Impatto: instabilita del punteggio e possibili errori tattici ai bordi dell'orizzonte.

[V]
### 2.2 Parallel root search: perdita di efficacia PVS/TT
- Nel ramo parallelo root i worker chiamano la ricerca con TT disabilitata (`useTT=false`, `allowTTWrite=false`) e finestra fissa full-window.
- Impatto: peggior scaling reale e perdita di forza ai depth medi/alti.

[V]
### 2.3 Politica ripetizione in search incoerente con draw ufficiale
- In search c'e gestione su `twofold` con bonus/penalita dinamica, mentre la draw ufficiale e `threefold`.
- Impatto: il motore puo distorcere la scelta linee in posizioni ripetitive.

### 2.4 UCI time management quasi assente
- `go` legge `movetime/wtime/btime` ma non li usa per allocare tempo reale.
- Impatto: in partita reale il motore e nettamente sotto-performante rispetto alla qualita potenziale della search.

## 3) Search: cosa manca / cosa aggiungere

## 3.1 Alta priorita (alto ROI Elo)

### A) Time management completo
- Soft stop + hard stop
- Allocazione tempo per mossa in funzione di:
  - fase partita
  - increment
  - stabilita eval
  - fail-low/fail-high
  - complessita posizione
- Panic time quando c'e crollo eval o nodo tattico critico

### B) Singular extensions TT-based
- Estendere selettivamente la hash move quando e chiaramente migliore delle alternative.
- Grande impatto in tattica forzata e sequenze quasi uniche.

### C) LMR moderno e contestuale
- Riduzione non solo su depth + moveIndex.
- Aggiungere segnali:
  - history score
  - improving/non-improving
  - tipo nodo (PV/non-PV)
  - check threat
  - quality del move ordering

### D) SEE pruning nel main search
- Oggi SEE e usata soprattutto per ordering/qsearch.
- Aggiungere pruning selettivo su catture chiaramente perdenti anche in search principale.

### E) QSearch con checks selettivi
- Oggi i check non-capture in qsearch sono esclusi per evitare esplosione.
- Aggiungere check generation limitata (es. first N moves / depth cap / SEE gate) migliora robustezza tattica.

### F) TT migliorata
- Salvare anche static eval in entry TT
- Usare TT anche per qsearch store (non solo probe)
- Migliorare cutoff e stabilita eval tra iterazioni

## 3.2 Media priorita

### G) Razionalizzazione draw logic in search
- Uniformare politica twofold/threefold e la parte "draw-avoid bias" in modo consistente e controllabile da parametro.

### H) Parallel search davvero YBWC
- Far lavorare i thread con policy di split piu vicina a YBWC reale (prima PV seria, poi siblings con finestre corrette e TT attiva).

## 4) Evaluation: cosa manca / cosa aggiungere

## 4.1 Alta priorita (alto ROI Elo)

### A) Tapered eval vera (MG/EG interpolation)
- Oggi hai bucket discreti opening/earlyMG/MG/EG.
- Passare a interpolazione continua per evitare salti artificiali di valutazione tra fasi.

[v]
### B) Pawn model piu ricco
- Backward pawns
- Pawn islands
- Connected passers
- Candidate passers
- Blocker davanti al passed pawn
- Rook behind passed pawn

[V]
### C) King safety moderna
- Shelter/storm model su file del re
- Penalita per file/diagonali aperte verso il re
- Safe checks / forcing checks pesati
- Saturazione corretta dell'attacco multiplo

### D) Threat model esplicito
- Minacce su pezzi non difesi o sovraccarichi
- Threat su donna/torre
- Possibili "next-move threats" (non solo stato statico attuale)

### E) Attack map da rifinire
- Verificare e correggere la composizione di `allAttacks` (inclusione del re difensore, coerenza con hanging/defended logic).
- Riduce errori di valutazione su pezzi "apparentemente appesi".

### F) Endgame scaling piu specialistico
- Regole specifiche per finali tipici (rook endgames, opposite bishops, fortress patterns)
- Eventuale probing Syzygy nei finali ridotti

## 4.2 Media priorita

### G) Pulizia e consolidamento feature overlap
- Evitare doppio conteggio implicito tra termini simili (es. king safety + king attack zone + hanging).
- Migliora tuning e stabilita.

## 5) Roadmap consigliata (ordine pratico)

### Fase 1 - Stabilita e correttezza
1. Correggere bug bound in qsearch delta pruning
2. Sistemare coerenza ripetizione/draw policy
3. Implementare vero time management UCI

### Fase 2 - Search strength
1. Singular extensions
2. LMR moderno contestuale
3. SEE pruning main search
4. Qsearch checks selettivi
5. TT enhancements (static eval + qsearch store)

### Fase 3 - Eval strength
1. Tapered eval continua MG/EG
2. Pawn model avanzato
3. King safety + threat model
4. Endgame scaling specialistico

### Fase 4 - Grande salto
1. Valutare integrazione NNUE (anche ibrida inizialmente con fallback HCE)

## 6) Validazione consigliata (obbligatoria)

- Tactical suite (STS o equivalente)
- Test regressione su posizioni critiche (zugzwang, fortini, ripetizioni, finali tecnici)
- Self-play SPRT tra branch
- Benchmark NPS + depth raggiunta a parita di tempo
- Confronto su time controls reali (blitz/rapid)

Senza questa pipeline, si rischia di ottimizzare localmente ma perdere Elo globale.

