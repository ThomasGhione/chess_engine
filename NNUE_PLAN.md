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
- [ ] Rete sacrificabile sui 16M (shakedown end-to-end, vedi Fase 2) — poi la
      v1 vera sui 100M.

### Fase 2 — training (serve GPU: Colab T4 gratis o noleggio)
- [x] **Setup bullet FATTO** (2026-07-06): crate `nnue/trainer/` (rev bullet pinnata,
      compila verificato). Arch: 768→256 **SCReLU** (standard bullet moderno, al posto
      del CReLU inizialmente scritto qui), batch 16384, lambda 0.7 (= bullet wdl 0.3),
      lr step decay, QA=255/QB=64. `trainer.rs` = training (Colab, `--features cuda` —
      bullet moderno NON ha backend CPU); `sanity.rs` = lettore standalone di
      `quantised.bin` (pure std) che fa da **specifica per il loader C++ di Fase 3**.
      Notebook pronto: `nnue/trainer/colab_shakedown.ipynb`.
- [ ] RUN shakedown su Colab (manuale: upload .zst su Drive, eseguire il notebook).
      Loss curve sana + sanity: eval(startpos) ≈ 20–50 cp, coppia mirror identica,
      ±donna enorme.

### Fase 3 — inferenza engine-side
- [ ] Accumulatore int16[2][256] su Board (o struct affiancata), update add/sub in
      `doMove`/`undoMove` accanto agli update PSQT incrementali; snapshot/restore in
      `MoveState` come gli altri campi incrementali.
- [ ] Forward pass AVX2 (CReLU + dot product int16→int32). Target: NNUE eval
      ≤ 2× il costo dell'HCE attuale (la rete recupera in qualità ciò che perde in NPS).
- [ ] Caricamento rete: file esterno con path UCI option + **embed nel binario**
      (incbin) come default, così `./chess` resta self-contained.
- [ ] Verifica correttezza: eval NNUE incrementale ≡ eval NNUE from-scratch su
      10k posizioni random (stesso pattern del check dual-representation di Board).

### Fase 4 — validazione e switch
- [ ] SPRT `[0, 5]` NNUE vs HCE (stesso binario, opzione on/off) — atteso largamente
      positivo; se < +50 c'è un bug (dati, quantizzazione o inferenza).
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
- **NPS**: sulla AVX2 del laptop una 768→256 costa ~1-2 µs/eval; l'HCE attuale con
  cache ne costa meno, quindi NPS calerà (~20-40%). È atteso e ampiamente ripagato:
  non farsi ingannare dal node-count — vale solo SPRT/gauntlet.
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
