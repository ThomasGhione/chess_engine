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

### 1. Datagen v2 — dati etichettati dalla rete v1
- [ ] Lanciare su tutte le macchine disponibili (branch `dev`, prefissi NUOVI
      per macchina, niente netPath: etichetta con la v1 embedded):
      ```sh
      git clone -b dev https://github.com/ThomasGhione/chess_engine.git && cd chess_engine && make prod
      setsid nohup ./chess datagen nnue/data/<nome-macchina> <threads> 8000 > nnue/data/datagen.log 2>&1 &
      ```
- [ ] **Target: ~250M posizioni totali** (≈4-5 giorni con 4 macchine; fermarsi
      a ~150M va bene per un ciclo più rapido). Stop/resume con Ctrl+C sicuro.
- [ ] Raccolta identica alla v1: zip/7z dei `data/` → laptop → estrarre in
      cartelle separate → `bullet-utils validate` su ogni file → `interleave`
      → validate del merged → `zstd -T0 -5` → Drive.
      (bullet-utils: ricompilare con `cargo b -r --package bullet-utils` in un
      clone di bullet — il binario della sessione scorsa era in scratchpad.)

### 2. Training v2 — hidden 512
- [ ] Adattare `trainer.rs`: `HIDDEN_SIZE = 512` (il resto invariato: SCReLU,
      QA=255/QB=64, SCALE=400, wdl 0.3, StepLR). Aggiornare gli static_assert
      di layout in `nnue/network.hpp` + `sanity.rs` (NETWORK_PAYLOAD_BYTES
      cambia) e il forward C++ (i loop sono già generici sulla dimensione;
      verificare solo le costanti).
- [ ] Notebook `colab_v1.ipynb` → `colab_v2.ipynb`: nuovo file dati, 40 SB,
      net_id `hydray-v2-512`. Su T4 la 512 costa ~2x la 256: se non sta in una
      sessione, i checkpoint intermedi ogni 10 SB già ci sono.
- [ ] (fallback economico) run parallelo a 256 sugli stessi dati: se la 512
      perdesse troppo NPS, la 256-su-dati-v2 è comunque un upgrade quasi gratis.

### 3. Validazione v2
- [ ] sanity.rs → swap `nnue/net/hydray.nnue` → `make prod` → `nnue-selftest`
      → nuovo baseline bench6 (ogni rete cambia l'albero).
- [ ] **NPS check** (macchina quieta, interleaved): la 512 raddoppia il costo
      di forward E update accumulatore. Se NPS crolla >25%, decide lo SPRT.
- [ ] SPRT vs 2.0.0: `NEW_OPTS="EvalFile=/abs/candidata.nnue" ./tuning/run_sprt.sh`
      (baseline = binario 2.0.0 congelato). Atteso +50-120.
- [ ] Gauntlet assoluto **ancorato su 2.0.0** (1.2.0/1.3.0 sono saturi).
- [ ] Release 2.1.0.

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
- [ ] **Nodi per mossa**: 8000 è il compromesso attuale. Con la v1 l'eval è
      più forte a parità di nodi; salire a 10-12k migliora le etichette a
      costo di ~25-40% di velocità. Da provare su UNA macchina e confrontare
      (due dataset piccoli, due reti shakedown, SPRT) — non cambiarlo alla
      cieca su tutte.
- [ ] **Diversità aperture**: oggi 8-9 ply casuali con filtro |eval| ≤ 400.
      Alternativa: seed dal book `books/openings.pgn` + 2-4 ply casuali.
      Valore incerto, misurarlo solo se v2 mostra segni di overfitting
      d'apertura sul gauntlet.

---

## Ciclo v3+ (dopo la v2)

- [ ] **Datagen v3** con la rete v2 (loop di rinforzo, ormai standard).
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
- [ ] Gauntlet: aggiornare la convenzione degli anchor a 2.0.0 nello script/uso.

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
