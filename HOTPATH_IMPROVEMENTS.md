# Hot-Path Improvements — analisi 2026-07-03

Analisi completa dell'hot path (searcher, sorter, movepicker, move_generator, TT, zobrist,
board doMove/undoMove/inCheck), evaluator escluso (futuro NNUE). Ordinata per criticità.

**Metodo di verifica**
- *Behavior-preserving*: node-count byte-identico via UCI (`Threads 1`, `Opening false`,
  `ucinewgame` + `go depth 12` su 6 posizioni miste). `make perf` misura TEMPI, non nodi.
- *Behavioral*: SPRT via `tuning/run_sprt.sh` (non-regression `ELO0=-3 ELO1=3`, gain `[0,5]`).
- Stime Elo da NPS: +10% velocità ≈ +7–10 Elo a TC fisso.

---

## 🔴 Criticità ALTA — problemi di performance reali nell'hot path

- [x] **1. Guardia `halfMoveClock >= 4` sul check ripetizioni** — Elo atteso: **+3–8** (NPS)
  ✅ 2026-07-03, node-identico. Con #2: +3.45% NPS (A/B interleaved 4×).
  `checkDrawTerminalConditions` chiama `countRepetitions()` (scan lineare O(historySize))
  a OGNI nodo, qsearch inclusa (50–80% dei nodi). Una ripetizione richiede
  `halfMoveClock >= 4`; in qsearch ogni mossa è cattura/promozione → clock quasi sempre 0.
  Aggiungere la guardia salta lo scan per quasi tutta la qsearch. Bonus endgame: scan
  all'indietro con early-exit a count 2/3. Node-identico per costruzione.

- [x] **2. Riposizionare il prefetch TT dopo `doMove`** — Elo atteso: **+2–5** (NPS)
  ✅ 2026-07-03, node-identico (vedi #1 per l'NPS combinato).
  `handleSearchPrelude` fa `prefetch(hashKey)` seguito subito da `probe(hashKey)` sulla
  stessa chiave: latenza zero nascosta (CLAUDE.md dice "2 nodes ahead" ma il codice non lo fa).
  Spostare in `searchMoves` e nel loop qsearch subito dopo `b.doMove()`:
  `tt->prefetch(b.getHash())` — la latenza si nasconde dietro enterNode+draw-check+eval del figlio.
  Node-identico.

- [x] **3. Unified TT probe: 1 snapshot per nodo invece di 4** — Elo atteso: **+3–6**, −40 LOC
  ✅ 2026-07-03 (bb9f5f8), node-identico. NPS **neutro** (−0.24% interleaved: i re-probe
  erano L1-hot) — tenuto per il valore strutturale (−40 LOC, sorter senza TT, un solo
  punto di lettura da estendere col campo static-eval per NNUE).
  Per nodo il bucket viene letto fino a 4 volte: `probe()` (prelude), `probeSE()` (static-eval
  tightening), `probeSE()`+`probeMove()` (gate singular), `probeMove()` (sortLegalMoves).
  Un unico `probeEntry(key) → {found, score, flag, depth, move}` a inizio nodo serve tutti;
  la hash move viene passata a `sortLegalMoves` come parametro (muore `outHashMoveIsLegal`).
  In tt.hpp `probe/probeSE/probeMove` collassano in una funzione. Node-identico (single-thread).
  Nota: qui si aggiungerà il campo static-eval nel payload quando arriverà NNUE.

- [ ] **4. Gate su `tryStalemateScore` nei cutoff RFP/NMP** — Elo atteso: **+2–5** (SPRT)
  Ogni fail-high RFP/NMP paga `hasAnyLegalMove()`. Stallo con eval ≥ beta+margine e
  materiale non-pedone è ~impossibile (RFP già gated `!isPawnEndgameForPruning`).
  Gate su materiale basso (es. `nonPawnMajorCount <= 4`) o rimozione dal path RFP.
  Behavioral → SPRT non-regression.

## 🟠 Criticità MEDIA — leve Elo non ancora provate (SPRT)

- [ ] **5. Riabilitare TT-write nei *discendenti* del probe singolare** — Elo: **+0–8**
  La SE passa `allowTTWrite=false` che si propaga a tutto il sottoalbero; solo il nodo con
  `excludedMove` ha la chiave "sbagliata" (soppressione locale già presente via
  `hasExcludedMove`). Passare `true` alla ricorsione del probe SE.

- [ ] **6. Negative extension nel blocco singular** — Elo: **+5–10**
  Il gruppo SE è stato la miniera del ciclo 2026-06 (SE_BETA_MARGIN +16.6). Ramo mancante
  standard: quando `seScore >= seBeta` e `ttScore >= beta`, ridurre di 1 il primo figlio
  invece di estendere. Unica leva SE mai provata.

- [ ] **7. Store bound-only sul stand-pat fail-high in qsearch** — Elo: **+0–8**
  Il return `standPat >= beta` esce PRIMA dello store. `store(hash, 0, standPat, LOWERBOUND)`
  senza mossa (bestMove=0 preserva la mossa esistente) evita la move-pollution che uccise
  l'esperimento qsearch-TT-con-mosse (−30 Elo).

- [ ] **8. `improving` con fallback a ply−4** — Elo: **+0–5**
  Quando `evalStack[ply-2] == NEG_INF` (in scacco 2 ply fa) `improving` è sempre false;
  fallback standard a ply−4. ~3 LOC.

- [ ] **9. Campagna SMAC3 sulle costanti di search** — Elo: **+15–40**
  Tutti i margini vinti a giugno (FUTILITY_MARGINS, LMP, HISTORY_PRUNE, SEE_CAPTURE, RFP,
  NMP_EVAL_*, PROBCUT_*, SE_*) sono guessed, not tuned. Serve esporli temporaneamente come
  UCI spin (oggi constexpr), tunare a gruppi di ≤8, ri-congelare.

- [ ] **10. TB probe dopo il TT probe** — Elo: **+0–3** (solo endgame)
  Il probe Syzygy (mmap) avviene prima del TT cutoff: in TB-range ogni nodo lo paga anche
  quando il TT avrebbe tagliato. Spostare il blocco dopo il prelude.

- [ ] **11. Lazy SMP al posto di YBWC** — Elo: **+50–100** (4 core), più su 8/16
  Già in piano (attesa hardware). SearchRuntime già pronto. Bonus: muoiono ~120 LOC di
  getBestMove (thread arrays, chunking, deferred re-search) e `allowHeuristicUpdates`.

## 🟡 Codice morto / duplicato — zero Elo, −LOC (node-count identico)

- [x] **12. Fondere `generateLegalEvasionsFor` in `generateLegalMovesFor`** — **−60 LOC**
  ✅ 2026-07-03 (9928ca3), node-identico, −47 LOC netti via CheckContext.
  Il generatore completo gestisce GIÀ lo scacco (evasionMask, double-check early-return,
  castling gated `!inCheck`); la versione evasioni è lo stesso codice meno il castling.
  Unificare con un parametro `CheckInfo {known, inCheck, doubleCheck}`.

- [x] **13. Rimuovere parametro `useTT` (sempre true) + `useHashMove`** — **−15 LOC**
  ✅ 2026-07-03, node-identico.
  Nessun call-site esterno passa mai false: `canUseTT` collassa in
  `runtime.transpositionTable != nullptr`. Idem `useHashMove` di sortLegalMoves
  (il null-check interno c'è già).

- [x] **14. Check re-legalità del re in qsearch morto** — **−4 LOC**
  ✅ 2026-07-03, node-identico.
  searcher.cpp ~1084: le mosse vengono da `generateTacticalMovesFor` che filtra GIÀ le
  catture del re con `isLegalPseudoMove`. Il re-check è sempre true.

- [x] **15. Campo `MovePicker::hashMoveIsLegal` mai letto** — **−6 LOC**
  ✅ 2026-07-03, node-identico.
  Scritto in sortLegalMoves, nessun reader (il searcher usa `outHashMoveIsLegal`).

- [x] **16. `MovePicker::size` duplica `moves.size`** — **−8 LOC**
  ✅ 2026-07-03, node-identico.
  Due campi da tenere in sync in 3 punti. Usare solo `moves.size`.

- [x] **17. Wrapper `Sorter::givesCheckFast` 1:1** — **−5 LOC**
  ✅ 2026-07-03, node-identico.
  Forwarding puro a `givesCheckAfterQuietMoveFast`: rendere pubblica quella vera.

- [x] **18. `betaOrig` in qsearch** — **−2 LOC**
  ✅ 2026-07-03, node-identico.
  `beta` non è mai modificato in qsearch; la copia è inutile.

- [ ] **19. Passare `inCheck` a `sortLegalMoves`** — +NPS piccolo
  sorter.cpp ricalcola `b.inCheck()` per nodo solo per il ramo opening-king;
  il chiamante lo conosce già. (Superato da #24 se si rimuove l'euristica.)

- [ ] **20. `checkersTo(kingSq)` unificato** — **−20 LOC**, +NPS nei nodi in scacco
  Oggi 3 scan di attacco per nodo in scacco: `inCheck()` → `isDoubleCheck()` →
  `computeCheckEvasionMasks` (ricalcola i checkers). Un unico bitboard risponde a tutte
  e tre (≠0, >1 bit, maschera evasioni).

- [ ] **21. Conversione promo-char triplicata** — **−25/−40 LOC** (invasivo)
  `Board::normalizePromotionChoice`+`promotedPieceFromChoice`, `Sorter::promotionPieceType`,
  `TT::Entry::promoCodeFromChar/CharFromCode`: tre versioni della stessa tabella.
  Soluzione elegante: promo come piece-type (2 bit) nel `Move` — refactor dedicato
  (tocca UCI/driver/FEN), non en passant.

- [ ] **22. Ponder-move da TT duplicato** — **−8 LOC** (cold path)
  `engine.cpp:getTTPonderMove` e `uci.cpp:ponderSuffix` fanno lo stesso probe+decode.

- [x] **23. Minori** — ✅ 2026-07-03 (1ad2d0f), node-identico
  - [x] `LMR_MAX_MOVES`/`MAX_MOVES`: cross-reference nel commento (unificarli costerebbe
        include-churn movelist↔search_constants — non vale la pena)
  - [x] `LMP_THRESHOLDS[..][6]`: entry [5] mai indicizzata → array [5]
  - [x] Commento stale su FUTILITY_MARGINS corretto (1..6)
  - [x] `TT::probe` const (poi assorbito da probeEntry in #3)
  - [x] hydray.md: claim "prefetch 2 nodes ahead" corretto insieme a #2 (2026-07-03)

## 🟢 Candidati "remove-to-gain" (SPRT; pattern vincente 3/3 a giugno: +19, +10.9, +8.8)

- [ ] **24. Opening-king penalty + castling bonus nell'ordering** (sorter.cpp ~265)
  Ultima micro-euristica di ordering sopravvissuta alla purga. Rimuoverla elimina anche
  il `b.inCheck()` per nodo (#19) e la lettura di fullMoveClock.

- [ ] **25. SEE cache 1 MB thread_local** (sorter.cpp ~130)
  64K entry × 16 B per thread. Con la lazy SEE le chiamate sono crollate: la cache
  potrebbe non ripagare la cache-pressure. Se neutra: −25 LOC e 1 MB/thread in meno
  (rilevante per Lazy SMP).

- [ ] **26. Layout killer `[2][MAX_PLY]` → `[MAX_PLY][2]`**
  killer0/killer1 dello stesso ply oggi distano 192 B (2 cache line). Micro, node-identico.

## ⛔ Da NON rifare (già testato e bocciato — vedi memoria sessioni 2026-06)

staged movegen (inerentemente non node-neutral) · razoring (×2) · cutNode / LMR-do-deeper ·
LMR da moveIndex 3 · contHist multi-ply nell'ordering · contHist in LMR/pruning ·
qsearch TT **con mosse** · estensione di scacco a tutte le depth · lazy eval ·
rimozione SEE per-quiet nell'ordering (load-bearing, +80% nodi senza).

---

## Ordine di esecuzione consigliato

1. **Behavior-preserving node-identici**: #14, #18, #15, #16, #17 → #13 → #1, #2 → #23 → #3 → #12, #20
2. **SPRT one-shot economici**: #4, #5, #6, #7, #8, #24, #25
3. **SMAC3 search constants** (#9, richiede esposizione UCI una-tantum)
4. **Lazy SMP** (#11, quando c'è l'hardware)

**Baseline node-count (6 posizioni, depth 12, Threads 1, Opening off): 7.922.716**
(startpos 1.257.726 · kiwipete 325.684 · endgame 17.810 · midgame 3.396.556 ·
tactical 869.242 · open 2.055.698 — driver: `nodebench.py`, vedi memoria tooling-nodebench)

**Log:** 2026-07-03 — batch #14/#18/#15/#16/#17 (d1d1dfe), #13 (8ba81c1), #1/#2 (0e3303c),
#23 (1ad2d0f), #3 (bb9f5f8), #12 (9928ca3): tutti node-identici (7.922.716 dopo ogni commit).
NPS: **+3.45%** dai batch 1–3 (A/B interleaved); #3 neutro (−0.24%, tenuto per struttura/LOC).
Prossimi: #20 checkersTo → SPRT batch (#4–#8, #24–#26).
