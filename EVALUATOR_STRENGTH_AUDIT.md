# Evaluator Strength Audit

Data: 2026-04-29

Scopo: aumentare la pura forza di valutazione dell'engine senza cambiare la search in questa fase. Le stime Elo sono range indicativi: vanno validate con SPRT/self-play, suite tattico-posizionali e confronto a fixed nodes.

## Executive summary

L'Evaluator e' gia diviso in termini chiari: materiale/PSQT incrementali, pawn structure, king safety, mobility, rooks, outposts, hanging pieces, trapped pieces e alcuni finali. La struttura e' buona per iterare.

I problemi piu importanti sono:

1. **Fase a bucket rigidi**: opening/earlyMG/MG/EG produce salti artificiali e rende tuning/cache piu fragili.
2. **Mancanza di tuning sistematico MG/EG**: molti coefficienti sono plausibili ma manuali; il salto Elo grosso arriva da PSQT/termini tuneati.
3. **Endgame troppo generico**: mancano scale factor, drawishness e finali specialistici.

## P1 - Massimo ROI Elo

### 1. Tapered evaluation vera MG/EG

- File: `engine/eval/evaluator.inl:88-95`, `engine/eval/general/eval_phases.cpp:5-101`
- Priorita: P1
- Elo atteso: +20 / +60

Oggi l'eval passa per bucket discreti: opening, early middlegame, middlegame, endgame. Questo crea discontinuita. Un singolo cambio materiale puo spostare molti termini insieme, non solo il valore di fase.

Come fare:

- Introdurre `Score{mg, eg}` o packed score.
- Ogni termine importante restituisce coppia MG/EG.
- Interpolare con phase material-based: es. N/B=1, R=2, Q=4, max phase 24.
- I termini puramente opening, come sviluppo e early queen, restano extra scalati dal move count o da "development phase", non mischiati col taper MG/EG.
- `evaluate()` diventa piu lineare: materiale + PSQT + termini, poi interpolation.

Perche aumenta forza:

- elimina salti di eval;
- rende i coefficienti tuneabili;
- migliora pruning basato su static eval;
- riduce falsi cambi di giudizio quando si entra/esce da endgame.

### 2. PSQT moderne e tuneate

- File: `engine/piecevaluetables.hpp`, `board/board.inl:354-376`
- Priorita: P1
- Elo atteso: +30 / +100

Le tabelle sono derivate da simplified eval/PeSTO come riferimento, ma non sembrano tuneate per il tuo motore. Inoltre solo pedoni e re hanno variante MG/EG; N/B/R/Q usano una tabella unica.

Come fare:

- Tabelle MG/EG per tutti i pezzi.
- PSQT dentro lo stesso sistema tapered.
- Tuning con dataset quiet positions + result labels, oppure Texel tuning su milioni di posizioni.
- Separare peso materiale e PSQT: non lasciare che una PSQT compensi un materiale troppo rigido.

Nota pratica:

- Questo e' probabilmente il singolo intervento piu redditizio ora che le correzioni di base sono state chiuse.
- Non va fatto a mano oltre una prima bozza: serve tuning automatico.

### 3. Endgame scaling e material signatures

- File: `engine/eval/general/eval_phases.cpp:64-101`, `engine/eval/rook/eval_rook.cpp`, `engine/eval/queen/eval_queen.cpp`
- Priorita: P1
- Elo atteso: +20 / +80

L'endgame attuale ha attivita re, passers, rook/queen pressure. Mancano pero scale factor e riconoscimento drawish.

Casi da aggiungere:

- insufficient material: KNNK, KBK, KNK, bare kings;
- opposite colored bishops: scale down forte, soprattutto con pedoni su un solo lato;
- rook pawn + wrong bishop;
- queen vs advanced pawn on 7th, con casi patta;
- rook endings: Philidor/Lucena semplificati, rook behind passer, king cut-off;
- lone queen/rook mate pressure solo quando il materiale e' davvero quel finale;
- fortress/pawnless endings;
- scale down quando il lato forte non ha pedoni e materiale insufficiente a forzare.

Come fare:

- Material hash/signature: conteggi pezzi + pedoni + colori alfieri.
- Funzione `scaleFactor(position, rawEval)` che riduce eval in finali drawish.
- Regole specialistiche prima semplici, poi Syzygy opzionale per <=5/6 pezzi.

### 4. Passed pawns piu concreti

- File: `engine/eval/pawn/evalPawnStructure.cpp:107-238`, `engine/eval/rook/eval_rook.cpp:32-59`
- Priorita: P1
- Elo atteso: +15 / +60

Hai gia passed, candidate, connected e blocked passer. Manca la parte che fa davvero vincere finali e posizioni strategiche:

- distanza dei re dal quadrato di promozione;
- rule of the square;
- passer protetto da re;
- blockader type: re/cavallo/alfiere/torre/donna;
- path clear e square controllate;
- passer esterno;
- passer remoto;
- passer con rooks behind correttamente pesato;
- candidate passer con lever enemy pawn.

Come fare:

- Bonus MG moderato, EG molto piu concreto.
- `passerScore = advancement + kingDistance + blockader + support + pathSafety + rookBehind`.
- In EG, aumentare molto quando il re avversario e' fuori quadrato o il re forte sostiene.
- Penalizzare passer bloccato da re avversario molto piu di un semplice pedone davanti.

## P2 - Miglioramenti importanti ma dopo P1

### 5. Mobility piece-specific e safe mobility

- File: `engine/eval/general/attack_data.cpp`
- Priorita: P2
- Elo atteso: +10 / +40

La mobility attuale somma mobilita grezza e divide per 2. Tutti i pezzi confluiscono in un numero unico.

Come fare:

- Tabelle mobility per pezzo e fase.
- Safe mobility: escludere square controllate da pedoni nemici, specialmente per minor pieces.
- Bonus per mobility centrale.
- Penalita per queen mobility precoce separata, non uguale a mobilita sana.
- Per alfieri/torri, distinguere raggi bloccati da propri pedoni vs pezzi mobili.

### 6. Outposts piu restrittivi

- File: `engine/eval/general/coordination.cpp:6-31`
- Priorita: P2
- Elo atteso: +5 / +20

Oggi outpost = pezzo supportato da pedone e non attaccato da pedone nemico. Manca:

- solo rank avanzati sensati;
- square nel campo avversario;
- impossibilita futura di essere cacciato da pedone;
- valore maggiore se vicino al re o al centro;
- bonus diverso per cavallo su d5/e5/c5/f5 e nero speculare.

Come fare:

- Outpost mask per colore.
- Bonus scalato da rank/file.
- Extra se il pezzo attacca key squares o re.

### 7. Bad bishop da struttura, non solo colore pedoni

- File: `engine/eval/bishop/eval_bishop.cpp:5-23`
- Priorita: P2
- Elo atteso: +5 / +25

Il termine conta pedoni propri sul colore dell'alfiere. E' una buona prima approssimazione, ma puo penalizzare troppo alfieri attivi fuori catena o in strutture aperte.

Come fare:

- Penalizzare solo pedoni bloccati/fissi sul colore dell'alfiere.
- Ridurre penalita se bishop mobility alta.
- Extra malus se tutti i pedoni sono su un lato e l'alfiere non controlla promotion square.
- Bonus per bishop pair deve dipendere da apertura della posizione e pedoni su entrambi i lati.

### 8. Rook/queen endgame pressure troppo generico

- File: `engine/eval/rook/eval_rook.cpp:68-166`, `engine/eval/queen/eval_queen.cpp`
- Priorita: P2
- Elo atteso: +5 / +25

I termini di pressione verso il re avversario sono utili, ma rischiano di attribuire molto valore anche a posizioni non forzanti. Ad esempio `edgeProximity = 7 - distToEdge` premia anche re centrali con valore non nullo.

Come fare:

- Attivare questi termini solo per material signatures precise:
  - K+R vs K;
  - K+Q vs K;
  - K+2R vs K;
  - varianti con pochi pedoni, ma scalate.
- Usare scala piu piatta fino a quando il re avversario non e' davvero vicino al bordo.
- Non sommare pressione rook/queen con generic king activity senza cap finale.

### 9. Initiative/tempo piu prudente

- File: `engine/eval/general/material.cpp:18-22`, `engine/eval/evaluator.inl:78-82`
- Priorita: P2
- Elo atteso: -5 / +10

Il bonus tempo fisso e' piccolo, ma in finali di zugzwang puo essere sbagliato. Non e' un grande problema, ma va controllato quando si fa tuning.

Come fare:

- Tempo MG: piccolo, tuneato.
- Tempo EG: 0 o molto condizionale.
- Disattivare in pawn-only endgames o finali zugzwang-prone.

## P3 - Qualita, tuning e performance

### 10. Eval trace e test per feature

- File: `engine/eval/general/trace.cpp`, `tests/`
- Priorita: P3 ma abilitante
- Elo atteso diretto: 0 / +10

Non vedo test specifici per Evaluator. Per aumentare forza senza rompere cose serve una rete.

Test minimi:

- coordinate center: d4/d5/e4/e5;
- eval simmetrica: posizione e mirror devono dare score opposto;
- stesso board con fullmove diverso, comportamento atteso esplicito;
- passed pawn: blocked/unblocked, king in square/outside square;
- opposite bishops scale;
- king safety: castled vs non-castled, file aperto, pawn storm;
- no-kings/mate sentinel gia coperto dal search, ma meglio testare.

### 11. Tuning pipeline

- Priorita: P1 come progetto, P3 come infrastruttura
- Elo atteso: +50 / +150 nel medio periodo

Senza tuning automatico, i nuovi termini rischiano di essere piu rumore che forza.

Pipeline consigliata:

1. Generare dataset da self-play e/o partite forti filtrate.
2. Tenere solo posizioni quiet o usare qsearch score come target.
3. Logistic/Texel tuning per coefficienti.
4. Separare train/test.
5. Validare con:
   - fixed-node match;
   - fixed-depth match;
   - SPRT time-control reale.

Metriche:

- NPS prima/dopo;
- eval cache hit rate;
- node count a profondita fissa;
- score stability per iterazione;
- WDL/self-play.

### 12. Cache e dati: dopo la correttezza

- File: `engine/eval/general/evaluate.cpp`, `engine/eval/general/attack_data.cpp`, `engine/eval/pawn/evalPawnStructure.cpp`
- Priorita: P3
- Elo atteso: 0 / +20, piu speed che eval pura

La cache eval e attack cache sono piccole/direct-mapped o quasi. Dopo la correzione della chiave eval, i prossimi passi sono:

- valutare 2-way/4-way per eval cache;
- misurare collisioni;
- tenere pawn cache 2-way ma gestire overflow stamp senza problemi;
- evitare di cacheare termini dipendenti da dati non inclusi nella chiave.

## Ordine pratico consigliato

### Sprint 1 - Base moderna

1. Score MG/EG packed.
2. Tapered eval materiale.
3. PSQT MG/EG per tutti i pezzi.
4. Primi coefficienti da PeSTO/SF-like, poi tuning.

Elo atteso: +40 / +120.

### Sprint 2 - Pawn/endgame

1. Passed pawn con king distance, blockader e rule of square.
2. Material signatures e scale factors.
3. Opposite bishops, wrong bishop rook pawn, insufficient material.
4. Rook behind passer piu preciso.

Elo atteso: +30 / +100.

### Sprint 3 - Tuning serio

1. Dataset.
2. Texel/logistic tuning.
3. SPRT automatico.
4. Pulizia coefficienti e rimozione termini ridondanti.

Elo atteso: +50 / +150, ma dipende moltissimo dalla qualita del dataset.

## Note finali

La priorita assoluta e' rendere l'eval pura, deterministica e phase-stable. Poi conviene trasformarla in un sistema tapered tuneabile. A quel punto ogni feature nuova diventa misurabile; prima di quello, molti coefficienti manuali possono sembrare buoni in una posizione e peggiorare il motore in self-play.

La traiettoria piu promettente non e' aggiungere altri dieci micro-termini subito, ma:

1. passare a MG/EG tapered;
2. tuneare PSQT e coefficienti;
3. aggiungere endgame scaling specialistico.

Questa sequenza e' quella con il miglior rapporto rischio/Elo.
