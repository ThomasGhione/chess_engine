# NNUE — piano di implementazione

Obiettivo: sostituire la valutazione handcrafted (HCE) con una rete NNUE addestrata
su self-play HydraY. Guadagno atteso: **+150–300 Elo** con la prima rete seria,
altri +100–200 con iterazioni successive (v2, più dati). È il singolo passo più
grande verso i 3000.

Principio guida: **v1 minimale che funziona end-to-end**, poi iterare. Niente
HalfKA/bucket alla prima rete: ogni pezzo di complessità in più è un posto in più
dove il primo tentativo può fallire senza capire perché.

---

## Architettura v1 (decisa, salvo sorprese)

```
input:  768 feature × 2 prospettive     (pieceType[6] × color[2] × square[64])
        accumulatore per prospettiva (side-to-move / non-stm)
L1:     768 → 256, int16, CReLU
output: (256+256) → 1, side-to-move relative, centipawn-scaled
```

- **Perché 768 semplice e non HalfKP/HalfKA**: nessuna dipendenza dal re → niente
  refresh dell'accumulatore alla mossa di re, solo add/sub incrementali. L'infra
  incrementale esiste già in `doMove`/`undoMove` (`updateIncrementalEvalForPiece`):
  l'update NNUE si aggancia negli stessi punti.
- Quantizzazione standard bullet: pesi L1 int16 (QA=255), output int16 (QB=64).
- Inferenza: AVX2 (i7-8650U la ha; `-march=native` già nel build). 256×int16 =
  512 B per accumulatore → sta in L1 cache.
- v2 (dopo che v1 è in produzione): HalfKAv2 con king-bucket, output bucket per
  material count, 512-1024 hidden. Ogni step ri-validato via SPRT.

## Trainer: bullet

https://github.com/jw1912/bullet — Rust, lo standard de-facto per motori non-Stockfish.
- Addestra su GPU (Colab T4 gratis basta per una 768→256: ore, non giorni) o CPU
  (fattibile ma lento, ~giorni per run piccola).
- Formato dati: **bulletformat** (`ChessBoard`, 32 byte/posizione) — compatto,
  convertibile da testo `FEN | eval_cp | wdl`.
- Loss: blend `lambda * eval + (1-lambda) * wdl`; partire con lambda 0.7–1.0.

---

## Fasi

### Fase 0 — prerequisiti engine-side (fattibili ORA, senza GPU) — ✅ FATTA 2026-07-04
- [x] **Seam di valutazione**: opzione UCI `UseNNUE` (check, default false); rifiuta
      l'attivazione finché `NNUE::networkLoaded()` è false (Fase 3). Branch
      `[[unlikely]]` in cima a `Evaluator::evaluate()` → `nnue/nnue.hpp`.
      Node-identity verificata: 5.782.300 (byte-identico al baseline).
- [x] **Modalità `datagen`**: `./chess datagen [outPrefix] [threads] [nodes]`
      (default `nnue/data/hydray 3 8000`) + `./chess datagen-dump <file.bin> [n]`
      per verifica a occhio. Codice in `nnue/` (namespace `NNUE`):
      * scrive **bulletformat diretto** (32 B/pos, trascrizione letterale di
        `ChessBoard::from_raw`, `nnue/bulletformat.hpp`); file per-thread
        `<prefix>.t<N>.bin` in append → stop/resume con Ctrl+C/SIGTERM sicuro
      * apertura: 8–9 ply casuali, filtro |eval| ≤ 400 cp alla prima search
      * search a nodi fissi via `runIterativeDeepening` + `maxNodes` (depth cap 32),
        TT 64 MiB e SearchRuntime privati per thread
      * filtri: come da piano + skip se la bestmove è cattura/promozione
        (target statico rumoroso)
      * adjudication: come da piano + draw se |score| ≤ 10 per 8 ply dopo ply 80,
        e a 400 ply; partite interrotte da stop = scartate
- [x] **Contatore/verifica**: progress ogni 30 s (posizioni, partite, pos/s, ETA 100M).
      Validato su 9.861 record: 0 errori strutturali (occ/nibble/re/score/result),
      distribuzione risultati simmetrica. **Rate misurato: ~193 pos/s con 2 thread**
      → a 3 thread ≈ 280–290 pos/s ≈ **4–5 giorni per 100M** (molto meglio della
      stima iniziale di 2–4 settimane).

### Fase 1 — dati
- [x] Datagen di fondo ATTIVO dal 2026-07-04 (3 thread, nice 19, ~280 pos/s a laptop
      scarico → ETA 100M ≈ 3.5 giorni, molto meglio della stima 2–4 settimane).
- [x] **Formato validato dal consumatore reale a 16.4M** (2026-07-06, anticipato dai
      30M previsti — prima si trova un bug di pipeline, meno compute si butta):
      `bullet-utils validate` su tutti e tre i file → **"No invalid positions!"**
      su 16.48M posizioni, W/D/L 38/22/39. Interleave ok
      (`nnue/data/hydray_16M_interleaved.bin.zst`, 243 MiB per Colab).
- [x] Rete sacrificabile sui 16M (shakedown end-to-end, vedi Fase 2) — poi la
      v1 vera sui 100M.
- [x] **Dataset v1 COMPLETO (2026-07-07): 121.017.428 posizioni**, tutte con
      etichette shakedown, da 4 macchine (laptop 30.2M + fisso Thomas 27.3M +
      portatile Thomas 23.1M + fisso Simone 40.4M). Tutti i 33 file validati
      con bullet-utils (0 invalidi, W/D/L 33/31/34), interleaved in
      `nnue/data/hydray_v1_121M_interleaved.bin` (+ `.zst` 1.8 GiB per Colab).

### Fase 2 — training (serve GPU: Colab T4 gratis o noleggio)
- [x] **Setup bullet FATTO** (2026-07-06): crate `nnue/trainer/` (rev bullet pinnata,
      compila verificato). Arch: 768→256 **SCReLU** (standard bullet moderno, al posto
      del CReLU inizialmente scritto qui), batch 16384, lambda 0.7 (= bullet wdl 0.3),
      lr step decay, QA=255/QB=64. `trainer.rs` = training (Colab, `--features cuda` —
      bullet moderno NON ha backend CPU); `sanity.rs` = lettore standalone di
      `quantised.bin` (pure std) che fa da **specifica per il loader C++ di Fase 3**.
      Notebook pronto: `nnue/trainer/colab_v1.ipynb` (ex colab_shakedown,
      aggiornato per il run v1 da 40 superbatch sui 121M).
- [x] **RUN shakedown su Colab FATTO** (2026-07-06, 16.4M pos × 10 superbatch, T4).
      Sanity sulla rete scaricata: startpos **+33 cp** ✓, coppia mirror **identica**
      (456=456) ✓, ±donna ±1300 ✓. Rete: `nnue/data/hydray-v1-shakedown.bin`.

### Fase 3 — inferenza engine-side — ✅ FATTA 2026-07-06
- [x] **Accumulatore** `NNUE::Accumulator` (int16[2][256], `nnue/accumulator.hpp`) su
      Board, agganciato in `addPieceToBB`/`removePieceFromBB` (ogni evento pezzo di
      doMove/undoMove passa di lì — stesso principio della dual representation).
      **Niente snapshot in MoveState**: add/sub sono inverse esatte, undo = update
      opposto. Refresh from-scratch nei rebuild (`refreshNnueAccumulator`); il bulk
      rebuild passa da `dispatchPieceBBUpdate` direttamente → zero doppi conteggi.
- [x] **Forward AVX2** (`nnue/nnue.cpp`): SCReLU via mullo+madd (esatto: |w|≤128
      verificato al load), fallback scalare identico a sanity.rs. Eval C++ ≡ sanity.rs
      al centipawn su tutte le posizioni di riferimento. NPS con NNUE ~1.36M (vs ~2M
      HCE): nel range atteso.
- [x] **Caricamento**: opzione UCI `EvalFile` (+ refresh accumulatore su load e su
      UseNNUE=true). L'embed incbin arriva in Fase 4 con la rete v1 vera.
- [x] **Verifica**: `./chess nnue-selftest <net> [games]` — incrementale ≡ scratch
      byte-per-byte su **61.815 posizioni** (300 partite random: catture/EP/arrocco/
      promozioni), do+undo inclusi; coppia mirror asserita. Node-identity a NNUE
      spento: 5.782.300 ✓.

### Fase 3.5 — datagen v1.5 (decisione 2026-07-06)
- [x] **Probe di forza shakedown-vs-HCE: +458 ±95 Elo (LOS 100%) @ 180 game** —
      la rete da 16M già domina l'HCE. Di conseguenza (scelta utente): datagen HCE
      fermato a 18.1M; **riavviato con etichette dalla rete shakedown**
      (`./chess datagen ... [netPath]`, nuovo prefisso `nnue/data/hydray2`).
      La v1 vera si allena su questi dati (loop di rinforzo: rigenera sempre col
      valutatore più forte). Bonus: **439 pos/s** (più veloce dell'HCE: adjudication
      più precoce) → ETA 100M ≈ **2.6 giorni**. Formato ri-validato con bullet-utils.
- [x] Datagen distribuito su 4 macchine, fermato a **121M totali** (2026-07-07).
- [ ] Training v1 (40 superbatch, `colab_v1.ipynb`) su Colab, poi Fase 4.

### Fase 4 — validazione e switch
- [x] **incbin FATTO** (2026-07-06, in anticipo): rete embedded nel binario via
      `.incbin` GNU as (`nnue/embedded.cpp` ← `nnue/net/hydray.nnue`, path stabile
      committato — per ora la shakedown, si sostituirà il file con la v1).
      `UseNNUE=true` senza EvalFile attiva l'embedded → binario self-contained
      (+395 KB). Validazione blob condivisa file/embedded. `run_sprt.sh` ora accetta
      `NEW_OPTS`/`BASE_OPTS` (opzioni UCI per-engine): lo SPRT formale è
      `NEW_OPTS="UseNNUE=true" ./tuning/run_sprt.sh`. Smoke 4 game: 4-0 NNUE.
- [ ] SPRT `[0, 5]` NNUE vs HCE (stesso binario, opzione on/off) — atteso largamente
      positivo; se < +50 c'è un bug (dati, quantizzazione o inferenza).
      Probe già fatto con la shakedown: +458 ±95 @ 180 game.
- [ ] Gauntlet assoluto vs 1.2.0 + 1.3.0.
- [ ] Switch default `UseNNUE=true`, release 1.4.0 (o 2.0.0). HCE resta nel codice
      finché non parte il datagen v2 (le reti successive si allenano su dati generati
      DALLA rete precedente, non più dall'HCE).

### Fase 5 — iterazione (v2+)
- [ ] Datagen v2 con la rete v1 (qualità etichette migliore → rete migliore).
- [ ] Architettura v2: HalfKAv2 + bucket. Richiede refresh accumulatore su mossa
      di re + `FinnyTable` (cache di accumulatori per bucket) — solo quando v1 è solida.
- [ ] EvalCache/pawn-cache HCE: rimuovere quando l'HCE esce dal hot path (−LOC).

---

## Rischi / note

- **GPU**: unico anello esterno. Colab T4 gratis basta per v1; in alternativa
  bullet-CPU su una notte. Nessun blocco reale.
- **NPS**: MISURATO 2026-07-06 (CPU pulita, datagen congelato): NNUE-on ≈ **2.0M NPS**
  vs ~2.2M HCE → solo **−9%**, non il −20/40% stimato qui. Due micro-ottimizzazioni
  provate e SCARTATE con misure: eval-cache hash davanti al forward NNUE (−2.7%: il
  probe costa più del forward AVX2; il costo vero — gli update accumulatore — sta in
  doMove comunque) e update fuso a delta-list (−11%: l'auto-vettorizzazione GCC dei
  loop immediati batte il passaggio fuso con branch). L'implementazione "naive" era
  già ottimale. Da fare ALLO SWITCH: flip degli hint `[[unlikely]]` sui branch NNUE.
  ⚠️ Protocollo bench NPS: congelare il datagen (`kill -STOP/-CONT`) — sotto carico
  le misure oscillano ±13% e producono guadagni fantasma.
- **Dati > architettura**: la qualità/quantità dei dati domina. Meglio 100M posizioni
  pulite su rete semplice che 20M su architettura sofisticata.
- **Niente dati Stockfish/Lc0**: self-play only — reti "proprie" e nessun problema
  di licenza/identità del motore.
- Il tuning HCE (gruppi `tuning/`) perde valore col passaggio a NNUE: non investire
  altro tempo in tuning eval oltre a ciò che serve per etichette datagen migliori.

## Ordine consigliato di partenza

1. Fase 0 completa (seam UCI + datagen mode) — ~1 sessione di lavoro
2. Datagen di fondo ON (Fase 1) — poi si continua con #9/#21/altro mentre accumula
3. A 30M: pipeline di prova end-to-end (Fase 2+3 su rete sacrificabile)
4. A 100M: rete v1 vera, Fase 4
