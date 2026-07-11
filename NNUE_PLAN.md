# NNUE — roadmap (post-2.0.0)

Stato: **la v1 è in produzione**. Release 2.0.0 (2026-07-07, tag su main):
rete (768→256)x2 SCReLU embedded nel binario, addestrata su 121M posizioni
self-play (4 macchine, etichette dalla rete shakedown), **+662 ±155 Elo vs HCE**
(SPRT 292W-5L-3D @ 300, LOS 100%). L'evaluator handcrafted è stato **rimosso**
(−3751 righe): NNUE è l'unica eval, `EvalFile` la sovrascrive per i test.
La storia dettagliata delle fasi 0–5 è nel git log e in questo file fino a
commit `329913f`.

Principio guida invariato: **dati > architettura**; ogni step validato con
SPRT prima del merge. Obiettivo: 3000 Elo.

---

## Ciclo v2 (in corso)

### 1. Datagen v2 — dati etichettati dalla rete v1 ✅ COMPLETO (2026-07-09)
- [x] Lanciato su tutte le macchine (branch `dev`, label v1 embedded, syzygy
      adjudication ON ovunque, tb-adj ~28% delle partite). Laptop sbloccato
      alzando il PL1 15→25W (464→675 pos/s).
- [x] **Target superato: 366.170.252 posizioni** (target era ~250M):
      fisso 10,3M+12,1M+28,7M · fisso2 97,5M · fede 103,3M · portatile2 114,3M.
- [x] Raccolta: validate per-file → interleave dei 61 file → validate del
      merged (0 invalidi, W/D/L 37/23/38) → zstd.
      **Pronto per Drive: `nnue/data/hydray_v2_366M_interleaved.bin.zst` (5,7 GiB).**
      ⚠️ I file portatile2 avevano 384 record azzerati (power-loss del laptop
      a metà scrittura): ripuliti con filtro re/occupancy PRIMA dell'interleave
      — validare sempre, il crash sporca i dati in modo silenzioso.

### 2. Training v2 — hidden 512 (in corso)
- [x] `trainer.rs` `HIDDEN_SIZE = 512` + `sanity.rs` allineato (b2845c9,
      cargo check ✓). Resto invariato: SCReLU, QA=255/QB=64, SCALE=400,
      wdl 0.3, StepLR.
- [x] Notebook `colab_v2.ipynb` (b2845c9): dataset 366M, 40 SB, net_id
      `hydray-v2-512`, clona branch `dev`. Su T4 la 512 costa ~2x la 256
      (~4-8 h); checkpoint intermedi ogni 10 SB già attivi.
- [ ] **USER: upload .zst su Drive → run colab_v2.ipynb → download quantised.bin.**
- [ ] All'arrivo della rete: aggiornare gli static_assert di layout in
      `nnue/network.hpp` (NETWORK_PAYLOAD_BYTES cambia) e verificare le
      costanti del forward C++ (i loop sono già generici sulla dimensione).
      NON prima: il binario attuale embedda la 256 e smetterebbe di caricarla.
- [ ] (fallback economico) run parallelo a 256 sugli stessi dati: se la 512
      perdesse troppo NPS, la 256-su-dati-v2 è comunque un upgrade quasi gratis.

### 3. Validazione v2
- [x] sanity.rs (layout OK, startpos +34, mirror esatta) → swap
      `nnue/net/hydray.nnue` → network.hpp `HIDDEN=512` → `nnue-selftest`
      (41.577 pos, incremental≡scratch) → **nuovo baseline bench6:
      3.943.540 @ d12** (2026-07-09).
- [x] **NPS check**: v2 1,21M vs v1 1,31M = **−7,5%** (interleaved, 3 run) —
      ben sotto la soglia del 25%; la 512 è quasi gratis (memory-bound).
- [x] SPRT vs 2.0.0 (binario ricongelato dal tag — il vecchio chess_baseline
      era un 1.3.0 stantio): in corso, **+41 a metà run**, trend H1.
      Log: `tuning/sprt_v2.log`.
- [x] Gauntlet assoluto **ancorato su 2.0.0** (1.2.0/1.3.0 sono saturi):
      **2.1.0 = 3055 ±18** (1000 game TC 4+0.04, 2026-07-10). Nuova scala:
      2.0.0 = 3000 fisso (agganciata via SPRT +662 vs HCE≈1.3.0≈2366);
      la vecchia scala 1.2.0=2000 resta valida per i tag pre-2.0.0.
- [x] Release 2.1.0 (tag su main, 2026-07-09), uci.cpp → 2.1.0.

---

## Miglioramenti al datagen (prima di lanciare run lunghe, se si vuole)

In ordine di valore atteso; nessuno è bloccante per il ciclo v2:

- [x] **Adjudication Syzygy FATTA (2119165, 2026-07-07)**: a ≤5 pezzi la partita
      si chiude col WDL del TB → etichette di risultato esatte e partite più
      corte (smoke: ~24% delle partite adjudicate). Guard: niente diritti di
      arrocco; risultati decisivi solo con halfmove clock ≤ 60 (il WDL ignora
      la regola delle 50 mosse; cursed/blessed → patta). Arg opzionale
      `[tbPath]` (default `engine/syzygy/files`); senza TB si disattiva da sola
      (il banner dice quale modalità è attiva); progress riporta `tb-adj`.
      ⚠️ I TB NON sono in git (939 MB): copiarli a mano sulle altre macchine.
- [x] **Nodi per mossa: RISOLTO 2026-07-11, si resta a 8000.** Il test da
      manuale (parità di wall-clock, due shakedown, SPRT) ha dato 12k = −85
      ±23 vs 8k: il −37% di posizioni non è ripagato dalle etichette
      migliori. Dettagli nel Ciclo v3.
- [ ] **Diversità aperture**: oggi 8-9 ply casuali con filtro |eval| ≤ 400.
      Alternativa: seed dal book `books/openings.pgn` + 2-4 ply casuali.
      Valore incerto, misurarlo solo se v2 mostra segni di overfitting
      d'apertura sul gauntlet.

---

## Ciclo v3+ (dopo la v2)

- [x] **Output bucket (8, material-count)** — FATTO 2026-07-10, branch
      `output-buckets` (13fd952 codice + 61e7ab3 rete): (768→512)x2→8,
      bucket = (popcount(occ)−2)/4, l1w trasposto, cross-check C++≡sanity.rs
      esatto. Rete `hydray-v3-ob512` (Colab 40 SB su dataset v2 366M):
      startpos +39, **SPRT vs 2.1.0: +22,7 ±10,3 (nElo 29), LOS 100%, H1
      @ 2710 game**. Nuovo baseline bench6: **5.230.424 @ d12**.
      ⚠️ Scoperto bug LATENTE preesistente: ~0,1-0,2% di partite a TC 4+0.04
      finiscono in 'connection stalls' (anche nello SPRT v2: 4+5 crash sui
      DUE lati); non riproducibile in replay singolo — caccia dedicata da fare.
- [ ] **Datagen v3** con la rete ob (loop di rinforzo, ormai standard).
      **A/B nodi/mossa DECISO 2026-07-11: si resta a 8k.** Test a parità di
      tempo macchina (4h/braccio sul fisso, 14,6M pos @ 8k vs 9,2M @ 12k),
      due shakedown ob da 10 SB, SPRT: **12k = −85 ±23 vs 8k, H0 in 822
      game** — più posizioni > etichette migliori, senza appello.
      (`tuning/run_nodes_ab.sh`; log `tuning/sprt_ab_nodes.log`.)
- [ ] **Architettura HalfKAv2 + king bucket** (input dipendenti dal re):
      richiede refresh accumulatore su mossa di re + FinnyTable (cache di
      accumulatori per bucket). Guadagno tipico +80-150 sull'arch semplice.
      Solo quando il pipeline v2 è rodato.
- [ ] Output bucket per material count (economico, spesso +10-20).

---

## Search sulla nuova eval (parallelizzabile col datagen v2)

- [ ] **RUN SMAC3 delle costanti di search** (HOTPATH #9, ora sbloccato:
      l'eval è quella definitiva): gruppi `tuning/groups/search_pruning.json`
      e `search_shape.json`, `cd tuning && ./run_tune_local.sh <gruppo>`
      (conda env chess-tuning39). ⚠️ NON in parallelo al datagen sulla stessa
      macchina: entrambi saturano la CPU e falsano le partite del tuner.
- [ ] Dopo il tune: copiare l'ottimo in `search_constants.hpp`, SPRT di conferma.

## Manutenzione / backlog

- [ ] `.claude/commands/hydray.md` (skill /hydray) è ferma all'era HCE
      (~2000 Elo, YBWC, evaluator handcrafted): riscriverla sullo stato 2.0.0.
- [ ] `make test`: da riscrivere da zero (deciso 2026-07-06, non urgente).
- [x] Gauntlet: convenzione anchor aggiornata a 2.0.0=3000 (default dello script,
      2026-07-10).

---

## Note operative (imparate sulla pelle, non perderle)

- **Bench NPS**: macchina quieta (stop datagen/match), run interleaved,
  preferire l'`nps` riportato dal motore al wall time. Sotto carico ±13-15%
  e guadagni fantasma.
- **Node-identity**: `.claude/skills/run-hydray/driver.sh bench6` — baseline
  2.0.0 = **4.735.578 @ d12**; ogni swap di rete stabilisce un baseline nuovo.
- **Dati**: mai mischiare prefissi/etichettatori diversi nello stesso file;
  validare SEMPRE con bullet-utils prima e dopo l'interleave; i dati si
  spostano via scp/zip, MAI via git (`nnue/data` è gitignored).
- **Solo self-play**: niente dati Stockfish/Lc0 (identità del motore e licenze).
- Dataset v1 archiviato: `nnue/data/hydray_v1_121M_interleaved.bin.zst` (1.8 GiB).
- Dataset v2 archiviato: `nnue/data/hydray_v2_366M_interleaved.bin.zst` (5.7 GiB);
  i sorgenti (`ext_*`, `portatile2.*`) restano finché la v2 non è validata.
