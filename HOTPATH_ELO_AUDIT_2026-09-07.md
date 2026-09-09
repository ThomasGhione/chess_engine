# HydraY: audit completo degli hot path e delle opportunità Elo

Data: 7 settembre 2026. Riferimento: `68f3d5dfb5fc2fd9c7c1b5d08a144ac481b84f09` **più le modifiche locali già presenti** in `searcher.cpp`, `searchruntime.hpp`, `search_constants.hpp` e il nuovo `corrhist.hpp`. I numeri di riga si riferiscono a questo working tree.

**La priorità che emerge è: correggere i contratti della search, eliminare aggiornamenti NNUE inutili, poi migliorare la selezione delle mosse e la qualità dei target della rete.** Il forward deep è già molto ottimizzato; aggiungere istruzioni SIMD o aumentare la rete senza considerare l'intera ricerca non è automaticamente un guadagno.

Questo è un audit statico: lettura di codice, configurazioni, strumenti, documenti e porzioni finali di log esistenti. Non ho compilato, avviato il motore, eseguito benchmark/perft/SPRT, profilato processi o scandito dataset/PGN voluminosi. Non ho modificato sorgenti, binari, reti, script o lo SPRT in corso. Un controllo aritmetico di poche operazioni ha verificato il controesempio TT sotto riportato; `python-chess` non è disponibile nell'ambiente e non è stato installato.

Non è possibile ricavare «tutti gli Elo ottenibili» dalla sola lettura. Qui sono censiti i percorsi critici, i difetti osservabili e le famiglie di intervento pertinenti. **Nessuna proposta nuova ha un guadagno Elo misurato in questa sessione.** P0–P3 indicano priorità, non Elo stimati: P0 = contratto/validità del risultato; P1 = primo investimento; P2 = sperimentazione successiva; P3 = marginale, freddo o già sfavorevole. Un bug raro può essere P0 per correttezza e valere quasi zero Elo.

Legenda evidenza: **C** = codice; **D** = dati storici del repository, non riprodotti oggi; **H** = ipotesi da misurare. Costo S = patch circoscritta; M = più componenti; L = refactor significativo; XL = training/datagen o campagna estesa.

## 1. Mappa dell'intero percorso critico

```text
UCI go → Engine::searchUCI → TimeManager / copia Board
  → Searcher::searchBestMove → reset parziale history / helper Lazy SMP
    → runIterativeDeepening → aspiration → getBestMove → root PVS
      → doMove: board + bitboard + hash + ripetizioni + NNUE eager
      → searchPosition
          enterNode / draw / mate-distance pruning
          TT cutoff → Syzygy → checkers
          static eval: TT eval oppure ensureClean/Finny + NNUE forward
          correction history / improving / singular probe
          NMP / RFP / ProbCut
          legal movegen → scoring → lazy SEE / selection
          LMP / futility / history / SEE pruning → LMR / PVS → ricorsione
      → quiescenceSearch
          enterNode / draw / TT → checkers
          evasioni oppure static eval / stand-pat / delta
          tactical movegen / SEE / ordering → ricorsione
      → undoMove: aggiornamenti NNUE inversi + ripristino Board
    → TT root / PV / stabilità tempo
  → stop + join helper → voto → bestmove
```

| Area | Stato osservato | Costo o rischio da investigare |
|---|---|---|
| Accumulatore | 2 × 1024 `int16_t`; fusione remove/add per spostamento ordinario; refresh lazy solo al cambio di base del re | Aggiornamenti anche nei figli che tagliano senza eval e negli undo; catture/promozioni con più passate |
| NNUE deep | `(768×4 → 1024)×2`, pairwise, `1024 → 16 → 1`, 8 output bucket | Pairwise, scansione gruppi non nulli, dot sparso, coda float; qualità della rete |
| NNUE shallow | Supportato via `EvalFile`; SCReLU AVX2 | Secondario rispetto alla rete incorporata; limiti numerici distinti |
| Search principale | PVS, LMR/LMP, NMP verificata, RFP, ProbCut, SE | Contratti TT/stack/root/exclusion; costo dei tentativi che non tagliano |
| Qsearch | TT raw eval, stand-pat store senza mosse, filtro SEE eager | Doppio ingresso all'orizzonte, SEE su mosse non consumate, abort/store, stalli |
| Ordering | Generazione completa; SEE principale rinviata al picker | Lavoro prima del primo cutoff; SEE ricalcolata; history poco discriminante per catture |
| Board | Caselle in nibble, bitboard, hash incrementale, history da 255 entry | Accessi `get/set`, stato null/repetition, contratti EP/patte |
| TT | 4 entry da 16 B per cache line, XOR e `atomic_ref`, raw static eval | Replacement, eval non salvate, contesto della regola delle 50 mosse |
| SMP | Runtime privato; TT condivisa; thread helper ricreati a ogni ricerca | Avvio/join, perdita Finny TLS, reset fra partite, statistiche solo main |
| Tempo/UCI | Watchdog separato; stabilità fra iterazioni | Ponderhit limitato, latenza di arresto, costo root evitabile |
| Syzygy | Probe dopo TT; ritorno immediato alla radice; solo patte in-search | Arrocco non filtrato nel wrapper; WDL/DTZ/clock; costo mmap condizionale |
| Training/datagen | Bullet fissato a una revisione; teacher self-play; filtri quiet | Validation assente, target autoreferenziali, distribuzione, quantizzazione |
| Build/test/tuning | O3, LTO, native/BMI2; diversi strumenti standalone | Riproducibilità, Hash fastchess, copertura dei contratti critici |
| Driver/FEN/perft/tools | Fuori dalla ricorsione competitiva ordinaria | Correttezza e costo di preparazione; non priorità NPS |

La rete `nnue/net/hydray.nnue` è di **6.425.664 byte**, esattamente il formato deep del loader. Il feature transformer su disco pesa 6 MiB di soli pesi. I due accumulatori contengono **4096 B** di valori; la contHist attuale contiene **576 KiB/thread**, la nuova corrHist **192 KiB/thread**. Sono dimensioni ricavate dalle dichiarazioni, non misure di cache miss. In particolare, una tabella più grande della L2 **non implica** che ogni accesso manchi la cache: nel sorter si riusa un blocco contestuale.

Le caratteristiche CPU visibili includono AVX2, BMI2 e AVX-VNNI. Non ho verificato il codice macchina di `chess`, quindi il percorso effettivamente compilato va registrato prima dei futuri A/B. Il target Windows `x86-64-v3` non abilita automaticamente AVX-VNNI.

## 2. Le otto leve da mettere davanti alle altre

| Ordine | Leva | Evidenza e ritorno atteso | Costo | Rischio |
|---|---|---|---|---|
| 1 | Contratti TT/root/stack e arresto | C: difetti concreti; ripristina affidabilità e rende interpretabili i successivi test. Elo ignoto | S–M | Cambi dell'albero, soprattutto matto/aspiration |
| 2 | Accumulator stack con aggiornamento realmente lazy | C/H: evita lavoro prima di TT/draw cutoff e il calcolo inverso negli undo; maggiore opportunità strutturale NNUE | L | Stato per prospettiva, cambi base, null, SE, copie Board |
| 3 | Fusione delle catture e delle promozioni | C/H: meno passate sui 4096 B dell'accumulatore; candidato circoscritto e riutilizzabile nello stack | M | EP, promozioni, re, ordine degli hook |
| 4 | Generazione e scoring per stadi | D/H: molte ricerche consumano una sola mossa; possibile risparmio di pochi punti percentuali, da verificare | L | Cambia ordine, LMR/LMP/history; vecchio tentativo scartato |
| 5 | ProbCut e SEE orientati al cutoff | C/H: ora catture non ordinate, SEE completo, nessun pretest qsearch separato | M | Falsi tagli e costo aggiuntivo se il filtro non seleziona |
| 6 | Sparsità NNUE basata sui nodi cercati; fallback AVX2 sparso | C/D/H: riordino e VNNI già presenti, resta il costo dei gruppi e delle piattaforme senza VNNI | M–L | Regressioni dipendenti dalla CPU; preservare esattezza |
| 7 | Validazione esterna al trainer e miglioramento dei target | C/D/H: training loss non basta; nuovi dati della stessa distribuzione e più budget hanno già deluso | M–XL | Leakage, costo GPU, guadagni di loss senza Elo |
| 8 | Correction history con semantica verificabile | C: la variante locale ha criticità di bound e di baseline; lo SPRT attuale va lasciato concludere | M | Reintrodurre memoria/lavoro senza segnale utile |

I documenti storici riportano `searchPosition` 11,7% self, qsearch 1,7% self, movegen 5,4% totale e scoring legale 3,3% (`ELO_ROADMAP.md:484`). **Non sommare percentuali self/inclusive e non attribuire tutta la NNUE al simbolo evaluate**: parte del costo sta dentro do/undo inline. Per un sottopercorso che pesa frazione `f`, accelerato di `s`, il limite del guadagno totale è `1 / (1-f+f/s)`. Senza `f` aggiornato, promettere +10% o +20 Elo è arbitrario.

## 3. Search: difetti concreti e correzioni prioritarie

### S01 - Il confronto TT deve usare il punteggio già ribasato

**P0 · C · costo S · rischio basso di implementazione, albero diverso.** `searcher.cpp:774` e `:1036` passano `tte.score` grezzo a `ttBoundCutoff`, poi fanno `scoreFromTT` soltanto nel return. Anche il commento di `searcher.hpp` codifica l'ordine sbagliato.

Controesempio: entry LOWERBOUND con score memorizzato **31990**, nodo a **ply 8**, **beta 31985**. Il codice taglia perché `31990 >= 31985`, ma restituisce **31982**, che non prova il cutoff. Esempio simmetrico per UPPERBOUND negativo. Calcolare una volta `ttScore = scoreFromTT(tte.score, ply)` e usare quello in **tutti** i confronti e return. Test mirati per lower/upper/exact, segni, ply e finestre vicino al matto.

Il codice di riferimento Stockfish converte il valore prima di usarlo nei cutoff. È un riscontro del contratto, non una giustificazione per copiarne le costanti. [Sorgente primaria](https://raw.githubusercontent.com/official-stockfish/Stockfish/master/src/search.cpp).

### S02 - `improving` a ply 2 legge uno zero, non la valutazione della radice

**P1 · C · S–M · rischio medio.** L'unica scrittura di `evalStack` è `searcher.cpp:850`, dentro `searchPosition`. La radice percorre `runIterativeDeepening → getBestMove → searchRootMoveScore`, che chiama `searchPosition` a **ply 1** (`:354`). Non passa da `searchPosition(..., ply=0)`.

Pertanto `evalStack[0]` nel normale percorso UCI/datagen resta zero per il thread nuovo. A ply 2, `improving` significa di fatto `staticEval > 0`, anziché miglioramento rispetto alla radice. Il commento `:811` descrive una correzione che questo call graph non realizza.

Usare uno stack esplicito per worker/ricerca, inizializzare la radice con una static eval coerente e marcare i valori non disponibili. Non serve azzerare MAX_PLY a ogni nodo. Il runtime dei helper è privato: spostare lo stack in uno stato **privato per worker** non introduce automaticamente una race. La nota attuale «mai nel runtime perché condiviso» è troppo generale.

Inoltre SE e verifica null rientrano **allo stesso ply** e possono riscrivere lo slot dell'antenato (`:395`, `:874`). Occorre decidere quali valori rimangono validi dopo questi probe e ripristinarli quando necessario. Registrare decisioni improving cambiate e nodi, poi SPRT.

### S03 - Il primo fail-high alla radice non interrompe il loop

**P0/P1 · C · S · rischio medio.** `getBestMove`, `searcher.cpp:1191`: `if (!isFirst && isBetaCutoff(bestScore, beta)) break;`.

Se la prima mossa supera beta durante aspiration, il codice cerca almeno un'altra mossa. Dopo avere aggiornato alpha oltre beta, un ulteriore scout che supera alpha può anche attivare un full-window re-search con finestra invertita (`:1182`). Terminare al cutoff anche sulla prima mossa; scartare sempre l'iterazione interrotta. Testare primo-move fail-high, fail-low e scout re-search con assert `alpha < beta` all'ingresso dei percorsi previsti.

Questo intervento è distinto dal precedente esperimento, fallito, di **allargare** l'aspiration window.

### S04 - Interruzioni possono produrre store TT o apprendimento da risultati incompleti

**P0 · C · S–M · rischio medio.** Nella qsearch, dopo la ricorsione e `undoMove` (`:1112`), manca il controllo `runtime.isInterrupted()`: il valore di fallback può aggiornare best/alpha e raggiungere lo store `:1125`. Il loop principale lo controlla correttamente a `:655`, ma troppo tardi per annullare gli store già fatti dentro qsearch. ProbCut e SE controllano il risultato senza una barriera esplicita immediatamente dopo il probe.

Ripristinare la Board, verificare interruption e propagare l'abort **prima** di usare score, TT o history. Evitare forward NNUE di emergenza durante lo srotolamento quando un risultato numerico verrà comunque scartato; preservare il contratto delle API senza flag d'interruzione. Test: arresto deterministico dentro qsearch, ProbCut, SE; niente nuovi bound derivati dall'iterazione troncata; scelta dell'ultima iterazione completata; latenza stop→bestmove.

### S05 - Excluded search: identità della mossa e contratto del sottoalbero

**P0/P1 · C · M · rischio medio-alto.** `searcher.cpp:530` esclude usando `sameFromTo`, quindi escludere `a7a8q` esclude anche `a7a8n/r/b`. Una prova di singolarità deve escludere la **mossa completa**, promozione compresa.

Altri punti da correggere o rendere espliciti:

- Il probe SE parte prima della validazione della hash move (`:866` contro `:967`). Il reset dell'estensione a `:978` non protegge un precedente return multi-cut `:884`.
- Nel nodo excluded restano RFP/ProbCut; possono restituire prima di cercare le alternative. Valutare un percorso dedicato con semantica controllata. Non propagare la restrizione ai normali discendenti: il ripristino dei loro TT store ha già vinto storicamente.
- Lo store Syzygy `:792` non verifica `hasExcludedMove`. Lo score di una ricerca sul set ridotto non deve essere pubblicato come ricerca completa; per i risultati TB distinguere ciò che è ancora valido dopo l'esclusione.
- `moveIndex` include la mossa esclusa e le mosse potate. La prima alternativa effettivamente cercabile può essere trattata come late e potata; `best` resta NEG_INF quando non si cerca nulla. Distinguere «nessuna alternativa legale» da «nessuna alternativa cercata» prima di concedere una doppia estensione.

Testare promozioni alternative, una sola alternativa, tutte le alternative potabili, hash move non valida, SE sotto stop e verifica che solo il nodo excluded eviti lo store ordinario.

### S06 - Patta per 50 mosse, stallo e contesto storico della TT

**P0 per correttezza; P2 per Elo atteso · C · M.** `checkDrawTerminalConditions` precede la verifica del matto. A halfmove clock 100 restituisce zero anche se la posizione è matto. Predisporre un caso quale `7k/6Q1/5K2/8/8/8/8/8 b - - 100 1` e verificare col generatore: controllo manuale, non eseguito oggi con il motore.

Inoltre la qsearch non distingue «nessuna tattica» da stallo (`:1091`); stand-pat/RFP/NMP possono uscire prima della generazione legale. Non aggiungere incondizionatamente una generazione completa a ogni nodo: il repository documenta quanto costano queste guardie. Usare casi mirati e un rilevamento selettivo, misurandone il costo.

La chiave TT non include il halfmove clock. Due posizioni uguali, con clock diversi, riusano score/bound di ricerca; controllare la patta corrente prima del probe non risolve tutte le differenze future. Conservare la raw eval condivisibile, ma limitare i cutoff dipendenti da clock/history quando necessario. Non aggiungere banalmente tutto lo storico alla chiave: distruggerebbe transposizioni utili.

Un'altra incoerenza del risultato root: il ramo `moves.is_empty()` di iterative deepening (`:1304`) calcola matto/stallo ma non imposta `terminalRoot` o `completedAnyDepth`. `searchBestMove` ricade quindi nel fallback (`:337`), rigenera la lista e sostituisce runtime.eval con NNUE. La mossa resta vuota, ma lo score terminale viene perso. Un risultato terminale esplicito risolve anche il lavoro duplicato; è distinto dal caso TB/voto M03.

### S07 - Null move e ripetizioni hanno un confine storico incompleto

**P1 per audit di correttezza · C · M.** `doNullMove` incrementa `nullPly` senza inserire history; `countRepetitions` sottrae `nullPly & 1` al punto di partenza (`board.cpp:330`). Questo corregge la parità subito dopo la null, ma dopo una **mossa reale** nel suo sottoalbero esiste di nuovo una entry corrente: con nullPly dispari il conteggio parte una entry prima e non conta neppure la posizione corrente.

La sequenza della history contiene una discontinuità, non una traslazione globale di parità. Separare «plies dalla più recente null» dal numero totale di null e non cercare ripetizioni attraverso una null come se fossero una partita legale. `copyFromBoard` (`board.inl:23`) inoltre non copia `nullPly`. Effetto pratico da misurare, nessuna attribuzione automatica agli Elo o ai crash storici.

Predisporre test di null→quiet→quiet, null→cattura che azzera la history, null annidate non consecutive, undo e copia. Non eliminare la guardia halfmove>=4 né ripetere l'ottimizzazione stride-2 già misurata.

## 4. NNUE: ridurre il lavoro del feature transformer

### N01 - Accumulator stack realmente lazy: investimento principale

**P1 · C/H · L · rischio alto, risultato numerico da mantenere identico.** `Board::movePieceOnBB` e add/remove (`board.inl:160`) aggiornano subito l'accumulatore. Solo il cambio king bucket/flip rinvia il lavoro. Un figlio che esce per TT, draw, limiti o altra condizione può avere pagato tutti gli update, più l'inverso nell'undo, pur non chiamando mai evaluate.

Progetto concreto:

1. Mantenere per ply un piccolo record di pezzi aggiunti/rimossi, basi delle prospettive e flag `computed[2]`.
2. `doMove` modifica Board/hash e inserisce il delta; non percorre HIDDEN.
3. `evaluate` trova l'antenato utilizzabile per ciascuna prospettiva e materializza solo ciò che serve, oppure usa Finny se cambia base o ricostruire costa meno.
4. `undoMove` torna allo stato padre senza update vettoriali inversi.
5. I probe allo stesso ply non devono fare push come se avessero mosso; null cambia STM e contesto di ricerca, non i pezzi. Definire reset e copie per root, datagen, PV e worker.

Non implementarlo come «memcpy di 4 KiB a ogni mossa»: si perderebbe buona parte del beneficio. Uno stack pieno da 64 plies contiene circa **256 KiB di soli valori per thread**, più metadati; il footprint aggiuntivo può peggiorare la cache. Valutare materializzazione selettiva, fusione di più delta e compromesso con uno stack a blocchi.

Contatori necessari: do/undo per MoveKind, eval invocate, NNUE forward effettivi dopo TT eval hit, figli chiusi senza materializzazione, prospettive materializzate, lunghezza catena, Finny diff/refresh, cache miss e tempo totale. Non basta osservare il numero di chiamate evaluate.

Il progetto di uno stack con `push/pop`, flag computed e aggiornamenti differiti è riscontrabile nell'implementazione NNUE di Stockfish. L'adattamento e il vantaggio per HydraY restano da dimostrare. [Sorgente primaria](https://raw.githubusercontent.com/official-stockfish/Stockfish/master/src/nnue/nnue_accumulator.h).

### N02 - Fondere catture, EP e promozioni

**P1 · C/H · M · rischio medio.** `boardapi.inl:216` sottrae la vittima, poi `:224` sposta l'attaccante: due passate. La promozione sposta il pedone e poi lo rimuove dalla destinazione per aggiungere il pezzo promosso (`:257`, `:69`): fino a quattro passate in promotion capture.

Usare delta finali per mossa:

```text
quiet:             A' = A - W(pezzo,from) + W(pezzo,to)
capture / EP:      A' = A - W(pezzo,from) - W(vittima,capSq) + W(pezzo,to)
promotion quiet:   A' = A - W(pedone,from) + W(promosso,to)
promotion capture: A' = A - W(pedone,from) - W(vittima,to) + W(promosso,to)
```

Per una cattura a basi pulite, l'attuale traffico logico sull'accumulatore è 2×(4 KiB letti + 4 KiB scritti) per make, altrettanto per undo. La fusione lo dimezza; i pesi rimangono da leggere. Questi sono byte dell'algoritmo, **non traffico DRAM misurato né speedup del 50%**. Per re/cambio base mantenere un fallback per prospettiva. Il kernel è riutilizzabile dentro N01: non sono due guadagni da sommare.

### N03 - Fusione dei diff Finny e aggiornamento del re nella stessa base

**P2 · C/H · M.** Il Finny attuale (`board.inl:235`) compie una passata HIDDEN per ogni pezzo aggiunto/rimosso e poi copia la riga. Raccogliere un piccolo elenco di delta e applicarlo a tile di neuroni, caricando la riga una sola volta per tile; scegliere rebuild da bias se il diff supera il costo dei pezzi attivi.

`updateMove` tratta **ogni** re come caso lento (`accumulator.hpp:153`), anche se bucket e flip rimangono invariati. Il caso stessa-base potrebbe usare la fusione ordinaria; misurare la frequenza per fase, soprattutto nei finali. Evitare di sporcare preventivamente prospettive che restano valide.

### N04 - Separare NNUE dalla mutazione generica della Board

**P2 · C/H · M–L.** Un delta NNUE esplicito consente di saltare gli update durante ricostruzioni PV, perft, validazioni, parsing di sequenze e root fallback. Il risparmio competitivo di questi siti è secondario; il vantaggio strutturale è impedire che ogni nuovo uso della Board paghi HIDDEN involontariamente. Non introdurre un flag globale che disabilita NNUE durante search concorrenti.

## 5. NNUE forward: cosa cambiare e cosa conservare

### N05 - Ottimizzare per gruppi non nulli sui nodi della search

**P1/P2 · C/D/H · M.** Il riordino `nnue/tools/reorder.cpp` usa FEN da partite. Il forward gira soprattutto su **posizioni interne alla ricerca**, comprese tattiche, non sulle sole posizioni effettivamente giocate. Campionare una piccola frazione dei forward effettivi, separata per output bucket e fase, e riordinare usando quel carico; validazione su partite/nodi esclusi dalla calibrazione.

La metrica utile è il numero di **gruppi da quattro** non nulli, non il numero di byte non nulli. La permutazione deve spostare insieme le coppie `j/j+512`, entrambi i bias, tutte le righe FT e le colonne L1 delle due prospettive. Il riordino già presente non è una nuova proposta da riscoprire. I dati nel suo commento (circa 67% gruppi nulli) sono storici.

Si può sperimentare un obiettivo pesato per bucket/costo reale e ottimizzazione della coattivazione durante training, ma quest'ultima **cambia la funzione** e richiede una nuova rete/SPRT.

### N06 - Fallback AVX2 sparso senza saturazione errata

**P2, P1 se la distribuzione importante usa solo AVX2 · C/H · M–L.** `nnue_deep.cpp:278` usa L1 dense in i16, mentre la via VNNI è sparsa. Un singolo `maddubs_epi16` su attivazioni 0..255 sarebbe scorretto per saturazione: il commento attuale ha ragione.

Esiste però una variante esatta da provare: separare `x` in `x & 127` e `x & 128`, eseguire due `maddubs`, allargare/sommare in i32 con `madd_epi16(...,ones)` e accumulare. Per una coppia di prodotti, i limiti sono rispettivamente `[-32512,32258]` e `[-32768,32512]` con pesi i8: ciascuno entra in i16. La somma dei due risultati deve avvenire **dopo** l'allargamento.

Questo permette di usare i gruppi sparsi e `l1wT` anche senza VNNI. Costa più istruzioni per gruppo; vince solo se i gruppi saltati ripagano scansione e broadcast. Confrontare dense e sparse sul target Windows/AVX2, includendo posizioni dense. Non diminuire QA a 127 sulla rete corrente per rendere facile maddubs: cambierebbe l'eval.

### N07 - Pairwise e dot: microarchitettura, non semplificazioni algebriche arbitrarie

**P2 · C/H · M.** Nella via VNNI esistono già packus, divisione intera esatta via moltiplicatore, NNZ raccolti nella stessa passata, pesi trasposti e otto accumulatori indipendenti (`nnue_deep.cpp:165–275`). Conservare queste proprietà.

Esperimenti sensati: streaming pairwise→dot per blocchi evitando h8/nnz completi; gruppi di unroll differenti; dense VNNI quando la densità rende lo sparse più caro; riduzione delle store della lista NNZ. Ogni alternativa compete per registri con pairwise, pesi e somme, quindi può peggiorare per spill o dipendenze. Misurare separatamente pairwise, raccolta NNZ, L1 e coda.

La coda float ha due divisioni vettoriali (`:137`) e una riduzione seriale di 16 prodotti (`:316`). Reciproco precalcolato, FMA, somma ad albero e conversione diversa da `lround` **non garantiscono stessi bit**. Trattarle come approssimazioni numeriche finché non si dimostra altrimenti; confrontare output quantizzato, soglie e search, non solo errore medio.

### N08 - Layout e dispatch della rete

**P2/P3 · C/H · S–M.** Il branch deep è `[[unlikely]]` (`nnue.cpp:227`) pur essendo deep la rete incorporata. Correggere o rimuovere l'hint nel prossimo intervento sul file; impatto probabilmente modesto e dipendente da layout/LTO. Una specializzazione al caricamento può evitare dispatch nel forward, ma una chiamata indiretta può costare di più: non assumere un guadagno.

`NetworkDeep` alloca sempre `l1w16` e `l1wT`, anche quando uno non serve. Derivarli solo per il backend scelto riduce memoria e startup; se la tabella inutilizzata non viene toccata durante search, non è automaticamente un guadagno cache/NPS.

Il forward shallow ha una sola catena di accumulo per metà (`nnue.cpp:94`): unroll con 2–4 somme è testabile per l'uso di reti shallow, ma non è la priorità della rete incorporata.

### N09 - Contratti C++ del caricamento e dell'accesso ai pesi

**P0 per validità del codice, P2 come leva Elo · C · M.** `nnue.cpp:142/:195` converte `NetworkDeep*` a `Network*`. Uguaglianza di offset e dimensioni non crea un oggetto `Network` né rende lecita in generale l'accessibilità tramite un tipo estraneo. Usare un vero membro comune `FeatureTransformer` o una vista con puntatori ai reali array, condivisa dai due formati.

`nnue_deep.cpp:245` legge `uint8_t h8[]` attraverso `int32_t*`. Allineamento corretto non basta per type accessibility; sostituire i load da quattro byte con `memcpy` verso un `uint32_t` o un'operazione SIMD dal contratto appropriato. Il costo va controllato nel codice generato; un memcpy di quattro byte normalmente è riconoscibile dal compilatore. Non usare `-fno-strict-aliasing` come ottimizzazione di progetto.

Anche l'overlay shallow sul blob incorporato va trattato con un lifetime esplicito, oppure copiato in un oggetto tipizzato al load. Non ho riprodotto una miscompilazione e non attribuisco a questi punti i crash storici. [Regole C++ sull'accesso attraverso tipi](https://eel.is/c++draft/basic.lval).

### N10 - Cambio EvalFile: invalidare tutte le cache dipendenti dalla rete

**P0 per cambio rete, P3 durante una partita a rete fissa · C · S–M.** `uci.cpp:270` refresha la Board dopo load ma non svuota la TT, che contiene score e raw eval della rete precedente. Anche la correction history apprende rispetto alla rete vecchia. Invalidare TT e stato appreso pertinente a tutti i worker dopo un load riuscito, senza perdere configurazione/limiti.

Finny usa l'indirizzo `activeNetwork` come identità. La successione A→B→C può riutilizzare l'indirizzo A; se Finny non è stato usato durante B, può scambiare C per A. Preferire una generation monotona della rete. Il loader deve inoltre validare i dati **prima** di pubblicare lo stato, controllare finitezza della coda float e avere un formato/versione esplicito per le future architetture. La sola lunghezza del file non distingue due reti diverse con payload di pari dimensione.

### N11 - Limiti numerici e selftest

**P1 per abilitare refactor NNUE · C · M.** Il forward deep L1 somma al massimo `1024×255×128` in valore assoluto: i32 è sufficiente. Il forward shallow ha invece `sum(t*t*w)` su 1024 neuroni per metà; il limite per singolo prodotto i16 verificato dal loader non dimostra che la **somma i32** non trabocchi. Né `Evaluator::evaluate` limita universalmente la raw eval alla banda non-matto. Validare separatamente FT, dot, scala finale, output e conversione TT. Nessuna evidenza oggi di overflow della rete incorporata.

Il selftest corrente confronta tutto `Accumulator` con `memcmp`, inclusa la coda di padding di una struttura `alignas(64)`. Confrontare campi/array significativi. Inoltre il probe do+undo non valuta il figlio fra make e undo (`selftest.cpp:105`), e ogni confronto ricostruisce da zero: non copre bene la vera sequenza **cambio base→evaluate→undo**, né lunghe catene lazy, sibling, null e reload. Aggiungere casi deterministici per queste transizioni prima di N01/N02. Per SIMD verificare il contratto di allineamento dei load, soglie 0/255, gruppi tutti zero/tutti attivi, tutti gli output bucket e confronto C++/Rust.

## 6. Search: modifiche strutturali e potature da sperimentare

### S08 - Un solo ingresso logico al nodo di orizzonte

**P1/P2 · C/H · M · rischio medio.** A depth<=0 `searchPosition` ha già contato il nodo e controllato limiti, terminali e patte (`:729–748`); chiama qsearch che conta e controlla di nuovo (`:1024–1028`). È lavoro duplicato, e anche doppio conteggio di una parte delle posizioni visitate.

Unificare il prologo oppure separare qsearch pubblica e corpo «nodo già entrato». Conservare ordine dei terminali e mate-distance narrowing. Se si cambia la contabilità, gli NPS prima/dopo **non sono comparabili direttamente**: misurare tempo e veri nodi espansi, confrontare score/bestmove/PV a depth fisso senza node cap, poi ricalibrare `go nodes` e budget datagen. Il doppio conteggio non dimostra doppio costo della qsearch: vale solo sulla transizione dalla search principale.

### S09 - ProbCut: prima ridurre il costo dei tentativi inutili

**P1 · C/H · M.** `searcher.cpp:915–936` genera tutte le tattiche, le visita nell'ordine del generatore, calcola SEE completo e cerca a `depth-4`. Non usa una lista ordinata per probabilità di cutoff, non filtra con informazione TT negativa sufficiente, non esegue una qsearch preliminare separata prima della ricerca ridotta. La TT viene prefetchata nei loop ordinari, ma non nei figli di ProbCut o null.

Proposte separate, una per A/B:

1. Scegliere prima hash capture valida e catture con miglior combinazione SEE/MVV/capture history.
2. A depth dove il probe ridotto è ancora una ricerca principale, usare una qsearch di conferma prima di spendere il probe più profondo. A depth 3–4 il codice già finisce in qsearch: evitare una duplicazione.
3. Escludere il tentativo quando una entry TT affidabile e sufficientemente profonda lo contraddice; non trattare un upper bound come exact.
4. Provare soglia SEE derivata dal deficit rispetto a `probcutBound - staticEval`, con conversione materiale/eval coerente. La soglia fissa attuale è una scelta euristica, non un bug aritmetico.
5. Numero massimo di candidati/probe dipendente da depth, dopo avere misurato il cutoff rate per rango.
6. Registrare un lower bound e una profondità coerenti quando il ProbCut riesce, invece del solo `return beta`; valutare l'interazione con replacement e hash move.
7. Prefetch dopo make anche qui, come microprova secondaria.

Misurare: opportunità, tentativi, successi, nodi spesi nei fallimenti, successo per prima/seconda/tarda cattura, gain a TC. Un ProbCut più selettivo può risparmiare CPU e perdere tattiche; uno più permissivo può avere un cutoff rate maggiore e costare più di quanto risparmia.

Il log della precedente modifica di scala è incompleto: non trasformare il vecchio +5,39 ±12,21 in un guadagno provato. Decidere separatamente se chiudere quel debito prima di aggiungere altra logica.

### S10 - Generation e MovePicker per stadi

**P1 come prototipo misurato, non adozione automatica · C/D/H · L.** Il codice genera e valuta circa tutte le mosse prima di consumare la prima (`searcher.cpp:956`, `sorter.cpp:211`); lazy SEE ha già ridotto parte del costo ma non generazione/history scoring.

Disegno: TT legale → catture buone/promozioni → killer/counter validati → quiet ordinate parzialmente → catture perdenti. Conservare una lista separata delle mosse già emesse e identità completa. La validazione delle mosse speciali deve includere promozione/arrocco/EP; controllare from/to non basta. Riutilizzare checkers e pin context fra stadi, così il refactor non moltiplica il setup.

Il dato storico è favorevole al **meccanismo**: 53,4% dei nodi osservati consuma una sola mossa; fra quei cutoff, 98,1% è capture/killer/hash/counter (`ELO_ROADMAP.md:534`). Il ~3% di runtime proposto lì è una stima, non un risultato del refactor. I nodi non pesano tutti allo stesso modo, quindi nemmeno `0,534×8,7%` è una previsione esatta.

Due vincoli specifici: il progetto documenta un precedente staged movegen respinto; e le bande attuali si sovrappongono (quiet fino a 11250, capture base 10000). Lo staging modifica l'albero per costruzione. Ha senso riaprirlo soltanto come implementazione diversa, con ablation del costo effettivamente evitato e SPRT, non come patch dichiarata node-identical. Il solo «TT prima di tutto» copre una frazione piccola e non realizza l'intero beneficio.

### S11 - Qsearch: ordinare e filtrare soltanto ciò che viene consumato

**P2 · C/H · M.** `sortTacticalMoves` (`sorter.cpp:286`) fa SEE su tutte le catture sopravvissute al primo delta gate; poi il picker potrebbe consumarne una sola. Spostare finalizzazione/filtro nel picker tattico e aggiornare il delta pruning con l'alpha corrente. Possibile risparmio di SEE e make/undo; ordine e albero cambiano se il punteggio definitivo determina una nuova selezione.

La lista passa da generatore a sorter e poi viene move-assegnata alla qsearch. Una API che scrive direttamente nel picker evita copie del prefisso live; confrontare assembly prima di attribuire un costo a copie che LTO/NRVO potrebbero eliminare.

**Caso particolarmente circoscritto:** le evasioni qsearch vengono partizionate forcing-first e ricevono tutte score zero (`move_generator.cpp:299`). `nextMove` comunque scandisce l'intero suffisso a ogni estrazione. Un percorso di consumo sequenziale mantiene esattamente lo stesso ordine e rimuove scansioni inutili. Stimare la quota di nodi in scacco prima di investirci molto.

Non aggiungere automaticamente la best move nei TT store qsearch: quell'esperimento ha già perso. Eventuali prove di **lettura** della hash capture per ordinare vanno tenute separate dagli store di mosse.

### S12 - LMR/PVS/IIR: intervenire su categorie dimostrate, non sulle costanti già sconfitte

**P2 · C/H · M–L.** Lo schema attuale riduce anche late captures, mosse evasive e quiet check non riconosciuti dal gate ristretto; reduction è sempre almeno uno. `iirActive` aumenta la riduzione delle mosse tardive, non riduce direttamente la profondità del nodo. Sono scelte, non errori solo perché diverse da altri motori.

Prima dei cambi raccogliere re-search rate, fail-high dopo LMR, profondità raggiunta, history e classe della mossa. Poi provare, separatamente:

- reduction frazionaria prima dell'arrotondamento, per evitare che piccoli cambi di statistiche siano sempre inerti;
- gestione diversa per capture buone/capture cattive, usando SEE/history già disponibili;
- riduzione minore per mosse quiet realmente forzanti identificate da dati, invece di eliminare LMR su tutte le mosse in scacco;
- una definizione esplicita di nodo cut/all, soltanto se supera il precedente tentativo negativo con evidenza nuova;
- evitare full-window PV re-search quando lo scout non ridotto ha già provato `score >= beta` (`:647`): verificare bound e utilità dell'exact score prima di applicare;
- memoria di score/nodi delle root moves per l'ordine delle iterazioni successive, preservando prima la migliore precedente. Oggi `fullSort` riordina la lista ruotata; nelle aspiration retry non si mantiene una classifica completa aggiornata.

La tabella LMR è limitata a depth 19, ma il motore può essere invocato più in profondità. Estenderla ha interesse per TC lunghi/infinite, subordinato alla frequenza reale; non è una giustificazione per aumentare MAX_PLY alla cieca.

### S13 - SEE a soglia e riuso del risultato

**P1/P2 · C/H · M.** `staticExchangeEvaluation` ricostruisce il least valuable attacker a ogni scambio, con lookup slider, produce un numero completo e limita la sequenza a 16 (`sorter.cpp:121`). Nei chiamanti di pruning serve spesso soltanto `SEE >= soglia`.

Introdurre `seeGe(move, threshold)` con uscite anticipate e insieme di attaccanti aggiornato incrementalmente; lasciare SEE numerico dove il valore determina la demozione o il punteggio. Il picker ha già calcolato SEE per molte capture, poi `searchMoves:571` lo ricalcola: un risultato associato alla **stessa entry del picker** può evitare il doppio calcolo. Non reintrodurre la vecchia cache hash SEE da 1 MiB/thread, già rimossa con beneficio. Misurare quanti ricalcoli restano dopo i gate correnti contro memoria extra nel picker.

La SEE attuale considera attaccanti geometrici: non tratta esplicitamente ricatture inchiodate, ricattura del re su casa difesa o promozioni durante gli scambi successivi. È una approssimazione; quando alimenta pruning può rigettare sacrifici validi o ammettere scambi falsamente buoni. Costruire casi per queste categorie e scegliere un compromesso a soglia. Non serve trasformarla in una mini-ricerca legale completa. [Esempio primario di SEE a soglia](https://raw.githubusercontent.com/official-stockfish/Stockfish/master/src/position.cpp).

### S14 - History: informazione più precisa prima di più tabelle

**P2 · C/H · M.** Capture history è indicizzata da `[side][to][victim][2]`: due attaccanti diversi sulla stessa vittima/casa condividono la statistica. Testare `[side][attackerType][to][victim]` con singola cella e gravity calibrata, oppure rimuovere il secondo slot con ablation. I due slot attuali sono due statistiche molto correlate, non una classifica esplicita di due mosse.

Le history quiet/contHist vengono aggiornate solo sui cutoff, con bonus quadratico. Provare bonus/malus con cap e dipendenza dalla depth effettiva, history bonus da TT cutoff affidabile, low-ply history o history contestuale agli attacchi **una alla volta**. Un nuovo array grande può costare più dell'informazione aggiunta. Continuation a più ply, senza altra motivazione, è già sfavorevole storicamente.

C'è anche un piccolo difetto nel tracking: quando `searchedQuiets[64]` o `searchedCaptures[32]` è pieno, il cutoff corrente non viene inserito, ma il calcolo del malus sottrae comunque uno (`searcher.cpp:681/:693`). Si risparmia erroneamente l'ultima mossa precedentemente registrata. Tenere traccia se la mossa cutoff è stata effettivamente inserita. Caso raro, priorità P3 per Elo.

### S15 - RFP/NMP, fail-soft e scala delle valutazioni

**P2 · C/H · S–M.** Conservare i gate e i margini correnti finché non c'è un segnale. RFP depth più ampio, razoring, estensioni di check generalizzate e allargamento dell'aspiration hanno già perso. Non «correggere» automaticamente tutte le costanti moltiplicandole per 2,6.

Esperimenti ancora distinguibili: gate NMP parametrico `+100`, verifica null con orizzonte di esclusione più rigoroso per zugzwang, return RFP smussato/fail-soft, dipendenza da improving o da una misura di errore NNUE già disponibile. La presenza di una eval più affidabile può consentire più pruning, ma deve ridurre **errori al confine**, non soltanto MSE globale.

Il delta-prune qsearch ritorna alpha. Restituire `standPat+margin` può essere una diversa euristica fail-soft, ma la margin non è un upper bound matematicamente dimostrato della posizione. Il vecchio rapporto la definiva «strictly valid» e «node-identical»: entrambe le qualifiche sono eccessive. Richiede un test di forza e dei casi tattici.

## 7. Correction history: valutazione della variante locale in SPRT

Il commit HEAD ha rimosso la vecchia correction history; `log_sprt_nocorrhist.txt` riporta **+3,29 ±2,65 Elo, H1 accettata, 26.166 partite**. Le modifiche locali reintroducono una variante gravity. Il log relativo osservato era in corso: **nessuna decisione anticipata sul suo risultato e nessuna modifica durante il test**.

| ID | Impatto / costo | Dove | Causa ed evidenza | Proposta | Rischio |
|---|---|---|---|---|---|
| C01 | P1 / S–M | `searcher.cpp:821–837`, `:998` | C: correction aggiunta dopo che un bound TT può avere sostituito raw eval; update impara contro questo valore | Tenere distinti raw eval, corrected static eval e TT search estimate. Apprendere contro la baseline scelta esplicitamente; usare il bound per pruning separatamente | Cambia segnale di apprendimento |
| C02 | P1 / S | `searcher.cpp:993` | C: `best > staticEval` non esclude un fail-low. Un upper bound sopra la static eval non prova sottostima | Gating per flag: EXACT sempre pertinente; LOWER solo se sopra la baseline; UPPER solo se sotto. Conservare alpha originale | Cambia frequenza degli update |
| C03 | P1/P2 / S | `searcher.cpp:995–998` | C: destinazione vuota non significa quiet: EP e promozioni non-capture passano | Riutilizzare classifyCapture e promotionType; esplicitare rispetto di `allowHeuristicUpdates` | Meno dati; da misurare |
| C04 | P2 / S–M | `corrhist.hpp:43`, `search_constants.hpp:117` | C: tre celle limitate a ±1024 e divisione per 64 ⇒ massimo **±48**. Cap ±256 irraggiungibile | Non sprecare un test alzando il cap. Se il segnale è troppo debole, studiare scala/learning rate/visite | Una correzione più grande può peggiorare pruning |
| C05 | P2 / M | `corrhist.hpp:56–72` | C/H: hash di bitboard aggregati; minor confonde N/B, major R/Q nelle stesse case; 192 KiB privati | Ablation pawn-only; hash separati per identità o hash incrementali solo se ripagano il costo per mossa | Più granularità può ridurre il campionamento per cella |
| C06 | P2 / S–M | `searcher.cpp:1072`, `SearchRuntime` | C: correzione qsearch senza clamp analogo al principale; reset/invalidation incompleti nei helper e reload | Unico contratto corrected eval e reset di tutti i worker quando cambia la rete/partita | Niente reset per nodo |

Misurare errore residuo **out-of-sample**: una cella deve prevedere meglio la prossima posizione visitata, non solo adattarsi alla stessa posizione. Contatori: lookup, celle visitate, bonus arrotondati a zero, correzione nonzero, distribuzione ±48, update per flag, decisioni RFP/NMP/LMP cambiate, costo memoria. A depth 1, residual piccoli vengono ancora troncati a zero da `/8`: gravity non elimina ogni dead zone.

## 8. TT: utilizzare meglio ciò che è già in cache

| ID | Impatto / costo | Dove | Causa / evidenza | Cambiamento da provare | Rischio |
|---|---|---|---|---|---|
| T01 | P1/P2 / M | `searcher.cpp:900–909`, `:935`, `:1091`; `tt.hpp:469` | C: molti nodi calcolano raw eval e poi escono senza conservarla; store superficiale rifiutato non integra eval mancante | Aggiornamento eval-only senza inventare un bound; oppure conservare raw eval quando il bound più profondo resta | Store extra, invalidazioni SMP, layout/flag |
| T02 | P2 / M | `tt.hpp:470` | C: qualsiasi EXACT può sostituire una entry più profonda, anche qsearch exact depth 0 | Separare conservazione della mossa/eval e politica del bound; provare depth margin per exact shallow | Vecchia informazione può risultare meno pertinente |
| T03 | P2 / S–M | `tt.hpp:484` | C: score replacement = age×256 − depth×4; un salto age domina quasi tutta la depth rappresentabile | Tarare rapporto age/depth, bonus PV/exact, eventuale refresh di age | Più write traffic o TT troppo conservativa |
| T04 | P2 / M | `tt.hpp:270`, `:439` | C/H: probe e store riscansionano quattro slot dello stesso bucket | Writer hint/indice candidato dal probe, con rivalidazione al momento dello store | Gli helper possono sostituire lo slot nel frattempo |
| T05 | P2/P3 / S | `tt.hpp:334`, `:149` | C: successo `madvise` è un consiglio, non prova huge pages effettive; soglia è 32 MiB, pagina esplicita 2 MiB | Registrare mapping/AnonHugePages e test solo a macchina libera | Misure errate se si confonde richiesta con backing |
| T06 | P3 / L | layout TT | D: entry già da 16 B, bucket già da 64 B; riduzioni Hash precedenti non favorevoli | Conservare il layout come baseline; compact keys solo con dati nuovi e stress SMP | Collisioni, perdita eval, replacement diverso |

La possibilità T01 è particolarmente connessa a N01: un hit sulla raw eval evita il forward ma, nel codice eager, **non evita gli aggiornamenti FT**. Prima e dopo lo stack cambiano i costi marginali. Se si aggiunge un flag eval-only, il probe non deve considerarlo un bound di ricerca né una prova SE.

L'XOR lockless con word atomiche è una scelta sensata; non aggiungere mutex per nodo. Evitare però formulazioni assolute come «mai correttezza»: le collisioni restano probabilistiche e un nuovo schema deve mantenere la verifica delle entry. Sono invece già presenti e da conservare: score rebasing in store, raw eval distinta dal search score, prefetch nei due loop principali e preservazione della hash move quando bestMove=0.

## 9. Board, move generation, ripetizioni e Syzygy

| ID | Impatto / costo | Dove | Causa / evidenza | Proposta | Rischio |
|---|---|---|---|---|---|
| B01 | P2 / M | `board.hpp:160`, `board.inl:54` | C/H: ogni lettura nibble richiede indice/shift/mask; scrittura read-modify-write sul rank | Confrontare `uint8_t squares[64]` mantenendo bitboard, con identico ordine mosse | +32 B storage; vantaggio non garantito |
| B02 | P2 / M–L | `Move`, `classifyMoveKind`, sorter/search | C/H: piece/capture/promotion vengono riclassificati in più componenti | Move encoding con tipo/flag o metadati condivisi nel picker; cambi separati da NNUE | Più byte/mossa e maggiore coupling |
| B03 | P2 / M | `move_generator.cpp:183`, `:255`, `:378` | C/H: pin setup ripetuto tra ProbCut e generazione completa; checkers talvolta rifatti nel figlio | Context di attacchi/pin per nodo, riusato quando valido | Calcolarlo su nodi che tagliano sarebbe regressione |
| B04 | P2/P3 / M | pawn movegen / `piece.hpp` | C/H: generazione pedoni per pezzo, lookup PEXT già presente | Pawn bitboard setwise, slider table compatta con espansione, specializzazione CPU solo dopo profilo | Perft/EP/pin; costo PDEP/PEXT e cache dipendente da CPU |
| B05 | P0 correttezza / S | `board.cpp:229–297` | C: `hasAnyLegalMove` non considera EP, se nessun'altra mossa esiste | Aggiungere EP legale al controllo esistenza, mantenendo early out | Freddo nel search attuale, non promettere Elo |
| B06 | P2/P3 / M | `board.cpp:314`, `:330` | C: history reset/overwrite/restore; scan completo delle occorrenze | Ricerca a soglia 2 o 3, limite da irreversibili/arrocco, eventuale repetition stack/cycle detector | Non confondere twofold nel search con draw alla radice |
| B07 | P0 correttezza / S | `syzygy.cpp:33`, `:61`, `:86` | C: guardia solo numero pezzi; diritti d'arrocco non passati all'API | Non probare posizioni con diritti di arrocco | Può ridurre hit TB in rari casi corretti |
| B08 | P2 / M–L | `searcher.cpp:751`, `syzygy.cpp:96` | C/D: WDL vincenti ignorate nel search per un vecchio problema di conversione | Separare ranking DTZ root e bound WDL interno con semantica clock corretta | Reintrodurre king shuffling o falsi win con regola 50 |

Caso utile per B05: `k7/2Q5/2K5/8/pP6/P7/8/8 b - b3 0 1`. Per geometria il re nero è bloccato, a4-a3 è impedita e resta `a4b3` en passant; il controllo esistenza attuale non la visita. È un caso costruito da validare con l'oracolo quando disponibile; non ho eseguito un motore per riprodurlo.

Nel wrapper Syzygy il solo cutoff draw non giustifica di eliminare il resto dell'integrazione: root DTZ risolve proprio una debolezza già osservata. Non considerare WDL con clock zero e un cutoff a halfmove 60 una prova universale di vittoria con la regola 50 nei target datagen: usare la distanza al prossimo azzeramento e il contesto delle ripetizioni dove necessario.

Non propongo modifiche interne alla decompressione/cache del codice C Pyrrhic senza un profilo che le renda rilevanti: il costo è condizionato a tablebase caricate e range della posizione. Le parti da controllare prima sono i contratti del wrapper, il numero di probe evitabili e page fault/latency.

## 10. Lazy SMP, gestione del tempo e protocollo

| ID | Impatto / costo | Dove | Evidenza e intervento | Rischio / misura |
|---|---|---|---|---|
| M01 | P1/P2 / M–L | `searcher.cpp:233–293` | C/H: slot persistenti, ma nuovi thread per ogni go. Un pool persistente conserva anche Finny TLS e riduce creazione/join | Lifecycle stop/reset/reload; misurare tempo per mossa breve e scaling, non soltanto ricerca lunga |
| M02 | P1 / M | `engine.cpp:82`, `searcher.cpp:265` | C: reset ricrea solo il runtime principale; history/killers/corrHist dei helper sopravvivono a ucinewgame | Reset esplicito di tutti i worker ai confini partita e cambio rete, distinto dal decay per ricerca |
| M03 | P1/P2 / S–M | `searcher.cpp:319–328` | C: il voto trasferisce solo bestMove e bestScore; completedDepth/bound e info/PV già emesse restano quelli main | Rendere coerente il risultato selezionato e l'ultima info; confrontare bound affidabili. Non eliminare il voto che ha già vinto |
| M03b | P0/P1 / S–M | `searcher.cpp:317`, `:1349` | C: risultato root TB lascia completedDepth=0 e terminalRoot=false; un helper che completa una depth può sovrascriverlo nel voto | Distinguere risultato provato TB da risultato ricercato; preservare la scelta DTZ indipendentemente dai tempi dei helper |
| M04 | P2 / M | `searchBestMove` / root TB | C/H: helper avviati prima del probe TB e dei terminali root. Risolvere terminali/TB prima del pool; valutare fast path per unica mossa in partita con clock | Preservare semantica di go depth/nodes/infinite e legalità della mossa |
| M05 | P1 per esperimenti / M | `SearchRuntime::maxNodes`, `searcher.cpp:139` | C: cap per worker e conteggio UCI solo main; totale può arrivare a circa Threads×N | Documentare contatori main/totali; eventuale budget globale a blocchi, senza atomica contesa per nodo |
| M06 | P2 / M–L | `time_manager.cpp:106`, `:180` | C/H: stabilità score/mossa già presente; avvio depth usa soglia di elapsed, non costo previsto della prossima iterazione | Stimare costo successivo e probabilità di completamento; confronto STC/LTC. Il vecchio node-effort TM ha perso |
| M07 | P1 funzionale / M–L | `engine.cpp:340–352`, `uci.cpp:482` | C: go nodes senza clock/depth è limitato anche a DEFAULTDEPTH; ponderhit non arma il tempo | Separare limiti depth/nodes/ponder, trasferire clock su ponderhit; test protocollo con budget controllato |
| M08 | P1/P2 / S–M | `time_manager.cpp:40–47`, `:98–103`; unwind search | C/H: movetime ha floor 5 ms; con pochissimo tempo soft può superare hard; arresto richiede anche unwind e join | Test budget 1–30 ms, stop immediato, p95/p99 deadline→bestmove; nessun test temporale durante SPRT |

I helper hanno Board/runtime privati, ma `helperSlots` è globale al processo. È adeguato all'attuale uso UCI serializzato; non rende automaticamente rientrante `searchBestMove` fra due Engine indipendenti. Un futuro pool va posseduto da un contesto motore o da un servizio con contratto esplicito. Datagen usa direttamente iterative deepening e non va accidentalmente spostato su questo stato globale.

M03b è più di una incoerenza della stampa: i helper vengono avviati prima del probe TB main, e non eseguono quel probe root. Se uno completa depth >0 mentre il main carica/proba le tabelle, il voto preferisce quel risultato al risultato TB di depth 0. Può quindi ripresentare una mossa meno favorevole alla conversione già risolta dal ranking DTZ. Non è stato riprodotto oggi; le condizioni del ramo sono esplicite nel codice. Un enum per la provenienza del risultato o un fast path TB precedente all'avvio dei helper evita di inventare una depth numerica artificiale.

`softResetHistory` dimezza centinaia di KiB per worker sul thread principale prima del lancio. È lavoro per ricerca, non per nodo: pool, first-touch sul worker e reset demand-driven sono opzioni soprattutto a TC corti. Un decay lazy per cella cambierebbe l'euristica e aggiungerebbe metadati al lookup; non è una sostituzione gratuita. Sulla CPU ibrida visibile, affinity/core class e carico concorrente possono dominare differenze di pochi punti percentuali. Nessuna modifica ad affinity o priorità dei processi è stata fatta oggi.

M05 ha anche un caso API: quando `searchPosition` usa il counter predefinito che aliasa `runtime.nodesSearched`, il controllo somma due volte lo stesso contatore. I normali chiamanti iterative deepening passano un contatore locale distinto; correggere il contratto evita che un harness futuro misuri budget diversi senza saperlo.

L'I/O UCI e la ricostruzione PV sono percorsi per iterazione, non per nodo. Il PV walker fa legal movegen e do/undo NNUE fino a 16 ply: lo stack lazy può risparmiare anche qui. Nei log storici è segnalata una perdita di connessione/crash non risolta; dall'audit statico non ne attribuisco la causa a TT, stop o SIMD. Prima di altre micro-ottimizzazioni, preservare codice di uscita, stderr e ultime richieste UCI del prossimo caso reale. Una partita persa per processo morto non è un semplice rumore di NPS.

## 11. NNUE: qualità dei dati, training e architettura

### D01 - La validation dichiarata non viene eseguita

**P1 · C/D · M.** `nnue/trainer/Cargo.toml:12` fissa Bullet a `cebc78a…`. In quella revisione il parametro test_set produce solo un messaggio di funzionalità non implementata. Il trainer shallow lo documenta (`trainer.rs:45`) e il deep passa None (`trainer_deep.rs:193`). Non è una nuova scoperta ignorata dal codice: è un limite già riconosciuto, che resta da colmare. [Sorgente Bullet della revisione effettivamente usata](https://raw.githubusercontent.com/jw1912/bullet/cebc78a093d92cbc87e56cfef049184c225270b0/crates/bullet_lib/src/value.rs).

Serve un valutatore esterno dei checkpoint con loss holdout e forward quantizzato identico a produzione. Separare train/validation **per partita o famiglia di aperture prima dello shuffle**: posizioni vicine della stessa partita in entrambi i set falsano la generalizzazione. Conservare provenienza/seme/teacher/hash rete e identità partita in un sidecar se il formato compatto non li contiene. Misure utili: loss globale, per bucket/materiale/fase, calibrazione della probabilità, disaccordi float/quantizzato, errore vicino ai margini di pruning, costo di inferenza.

Non aggiornare Bullet soltanto per avere un flag validation: cambierebbe anche il backend e potenzialmente il training. Il confronto esterno permette prima un A/B controllato. Miglior loss è un filtro economico per decidere quali checkpoint giocare, non una prova Elo.

### D02 - Migliorare il teacher e la distribuzione visitata

**P1/P2 · C/H · XL.** Datagen usa per default 8000 nodi/mossa e target depth cap 32. Il target fonde 70% score del teacher e 30% risultato della partita; le adjudication dipendono in parte dallo stesso motore. Esiste quindi un circuito di autoapprendimento che può conservare errori del teacher. Non equivale a dire che self-play non funzioni: significa che ripetere lo stesso processo più a lungo può esaurire il beneficio.

Interventi da confrontare a budget dati/GPU definito:

- Relabel di un sottoinsieme difficile con teacher più profondo o più forte, mantenendo unità score/WDL coerenti. Preferire errori ad alta frequenza nel search o alta sensibilità delle decisioni, non soltanto gli errori più grandi in posizioni già vinte.
- Miscela di opening seed, teacher/network precedenti e budget di ricerca. Un piccolo mix controllato può ampliare le posizioni senza alterare tutto il corpus.
- Target WDL tablebase rispettosi del clock per i finali; trattare separatamente risultati conclusi e adjudicati. Il semplice cutoff halfmove <60 non dimostra una conversione vincente in ogni finale.
- Distillazione sulle posizioni quiet raggiunte alla fine di varianti tattiche. I filtri attuali scartano scacco, bestmove capture/promotion e score estremi: non eliminarli indiscriminatamente, ma misurare quali decisioni frequenti restano prive di target affidabile.
- Valutare lambda WDL e clipping in funzione della qualità del teacher tramite ablation; non modificare insieme scala eval, filtri, loss e rete.

Datagen include già 8/9 ply casuali, global shuffle del corpus e seeding di finali. Proporre «aggiungere shuffle» o «aggiungere finali» come se mancassero sarebbe errato. Il seeding è una partita ogni otto, 3–6 pezzi, 65% sbilanciato e massimo otto record; questo **non implica** 12,5% dei record nel bucket 0. I commenti sull'antico bucket mai addestrato descrivono una rete precedente: verificare distribuzione e residui della rete corrente, senza ripetere la vecchia ipotesi di buco dati già smentita nei documenti recenti.

### D03 - Quantizzazione e checkpoint devono ottimizzare il motore esportato

**P1/P2 · C/H · M–XL.** Training deep: MSE dopo sigmoid, feature factorizer, CReLU pairwise, L1 e testa; export FT i16, L1 i8, testa float. In produzione il pairwise usa divisione/troncamento intero esatto per 255, oltre a clipping e quantizzazione dei pesi. Confrontare:

1. modello float del checkpoint;
2. export quantizzato con riferimento scalare;
3. kernel AVX2/VNNI di produzione.

Il confronto 2↔3 deve essere esatto per una ottimizzazione aritmeticamente equivalente. Il divario 1↔2 è invece un problema di training/export: misurarlo per fase e attivazione, poi valutare quantization-aware training o fine-tuning che emuli i passaggi interi. Saturazione appresa e errore di clipping vanno misurati, non corretti a mano cambiando QA/QB nella sola inferenza.

Lo schedule attuale usa batch 16.384, 6104 batch per superbatch, WDL 0,3 e StepLR con salto a metà budget. Lo stage/resume conserva gli indici globali e il progetto ha già corretto vecchi problemi di ripartenza: non riproporli come bug attuali. In una nuova campagna, confrontare a budget campioni simile uno schedule più graduale, checkpoint intermedi selezionati su holdout e più seed dove serve stimare la variabilità. Budget 320→640 senza nuovo segnale non è la prima spesa utile.

### D04 - Architettura: frontiera fra accuratezza e costo dell'intera ricerca

**P2/P3 · H · L–XL.** Le opzioni ragionevoli rimaste non hanno tutte la stessa priorità:

| Opzione | Perché potrebbe aiutare | Costo e condizione per procedere |
|---|---|---|
| L1 da 16 a 32, FT invariato | Più capacità non lineare dopo pairwise | Raddoppia pesi/dot L1; kernel attuale assume 16. Generalizzare e misurare prima di un training lungo |
| Distillazione in FT più piccolo | Meno costo in ogni update e refresh, più cache disponibile | Accettabile solo se perdita di accuratezza è compensata da ricerca più profonda; confronto a tempo fisso |
| Sparsità/group sparsity appresa | Ridurre gruppi nonzero realmente pagati dal kernel | La penalità deve riguardare gruppi da 4 e nodi visitati, senza azzerare informazione critica |
| Skip/residuo lineare appreso dal FT | Segnale lineare economico aggiuntivo alla testa pairwise | Allenare e quantizzare congiuntamente; non sovrapporre materiale HCE manuale alla scala NNUE |
| Testa intera anziché float | Eliminare conversioni/divisioni/coda float | Serve analisi numerica e retraining/fine-tuning; non dichiararla bit-identica a priori |
| Bucket condivisi o transizione diversa | Ridurre dati richiesti per testa e discontinuità fra fasi | Modifica di modello completo; interpolare due forward potrebbe costare più del vantaggio |
| FT più ampio, nuovi input/threat features | Maggiore capacità e accesso a interazioni mancanti | Aggiornamenti più cari e più stato di attacchi; soltanto dopo lazy FT e residui che dimostrino il bisogno |

Il precedente passaggio a più input king bucket ha perso; non riproporre genericamente 4→8. Il progetto ha già ottenuto vantaggi da deep, riordino e sparsità, mentre più dati della stessa distribuzione e più training mostrano rendimenti decrescenti. Questi risultati restringono le ipotesi, non dimostrano che la capacità attuale sia ottima per qualsiasi teacher o CPU.

`nnue/tools/reorder.cpp:213–253` scrive il file destinazione **prima** del collaudo. Un test fallito lascia quindi un file già pubblicato e dichiarato non valido. Per future trasformazioni: destinazione temporanea, rilettura/validazione, rename finale. Non sovrascrivere una rete usata da processi attivi. Nessuna trasformazione è stata lanciata oggi.

## 12. Build, benchmark e affidabilità degli esperimenti

### I01 - Hash del ramo fastchess non viene passato

**P1 per validità esperimenti · C · S.** `tuning/run_sprt.sh:126` stampa HASH, ma le due specifiche motore `:145–146` passano Threads e opzioni aggiuntive, senza `option.Hash`. Il ramo cutechess lo passa a `:167`; `run_sprt_hash.sh` configura esplicitamente i due valori e non ha questo difetto.

Con fastchess viene quindi usato il default UCI del binario, salvo override in NEW_OPTS/BASE_OPTS. Se coincide con HASH non cambia il test; se differisce, il riepilogo mente sulla configurazione. Non ne consegue che tutti i risultati storici Hash siano invalidi. Correzione futura: default Hash esplicito per entrambi i motori con precedenza documentata degli override e manifest delle opzioni effettive. Lo script e lo SPRT attivi restano intatti.

### I02 - Rendere riproducibile ciò che si confronta

**P1/P2 · C/H · S–M.** Ogni A/B dovrebbe conservare due binari immutabili, SHA della rete incorporata/esterna, commit **e diff locale**, compilatore/flag/ISA, Hash/Threads/TB, TC/book, seed, modello SPRT e adjudication. Il commit da solo oggi ometterebbe proprio la nuova corrHist. Non riutilizzare `./chess` come artefatto mutabile di un test mentre una build successiva lo rimpiazza.

Il backend fastchess seleziona `model=normalized`: i parametri di ipotesi del modello e l'Elo riportato in una diversa calibrazione non vanno scambiati. Una calibrazione contro livelli Stockfish UCI_Elo non è una misura FIDE assoluta. Per trend interni usare lo stesso protocollo e opponent; per generalizzazione aggiungere in seguito un piccolo pool indipendente e TC più lungo.

### I03 - Build già aggressiva; niente PGO rituale

**P2/P3 · C/D · M.** Sono già presenti O3, LTO, native/mtune, funroll-loops e requisiti BMI2. PGO ha perso due volte. Prima di riprovarlo servono dati nuovi: corpus di profilo rappresentativo, ISA effettiva e attribuzione del precedente peggioramento. Anche togliere `-funroll-loops`, limitare inline freddo, separare loader/diagnostica e generare varianti ISA sono ipotesi da verificare su dimensione testo, frontend stalls e tempo totale. Non presumere che più unrolling o più inline equivalgano a più Elo.

Per il kernel sparse AVX2/VNNI, controllare disassembly e spill a macchina libera. Non eseguire tuning microarchitetturale insieme allo SPRT: frequenza, temperatura e pressione sulle cache renderebbero difficile interpretare entrambi.

### I04 - I test esistono, ma vanno associati al contratto cambiato

Perft suite verifica legal movegen/do-undo, non TT bound, stop o forza. NNUE selftest e deepcheck coprono parte dell'incrementalità e dell'equivalenza SIMD; vanno integrati con le sequenze N11 prima dello stack. Il target perf registra le suite di `engine/test/performance-test/performanceEngine.cpp`: ricerca depth 11, self-play depth 10, eval e movegen. Il microbench eval ripete 16 Board immutate otto milioni di volte: misura forward caldo, non il costo NNUE incrementale nella ricerca. Le soglie assolute di durata dipendono da CPU/carico; non sostituiscono un A/B. Non ho eseguito questi test.

Parsing FEN/UCI, driver ASCII, script di plotting, serialization dei dati e shuffle sono stati considerati nel percorso complessivo: non consumano la ricorsione di una ricerca UCI normale. Migliorarli può ridurre setup/training wall time o errori dei test, ma non giustifica una promessa NPS. Il perft attuale aggiorna comunque NNUE nei do/undo: separare i consumatori Board-only aiuta gli strumenti, senza contarlo automaticamente come guadagno competitivo.

### I05 - Configurazioni di build condividono oggetti e binario

**P1 per validità esperimenti · C · M.** `makefile:3`, `:155–158`, `:187–192`: debug e prod usano lo stesso output/ e lo stesso chess, ma debug aggiunge `-DDEBUG -g -pg -O1`. Make segue timestamp, non la variazione dei flag: un passaggio di configurazione può non ricompilare o riutilizzare oggetti incompatibili con il nome del target. Separare directory/binari per configurazione, oppure introdurre una dipendenza dal fingerprint del comando. La dipendenza esplicita della rete embedded è invece già presente e va conservata.

Il target Windows include `-DDEBUG` nei flag prod (`makefile:43`), attivando anche il controllo previousMove nel percorso search (`searcher.cpp:942`). Verificare che sia intenzionale; mantenere diagnostica nel binario di debug. I test usano gli stessi oggetti dei moduli e TEST_FLAGS al link: non assumere che una macro passata solo al link sia stata applicata a tutte le unità durante compilazione.

## 13. Interventi già tentati, già presenti o di scarso rendimento

Questa tabella serve a evitare di spendere la prossima campagna sulle stesse ipotesi. Dati da `ELO_ROADMAP.md:394` e log/documenti consultati; intervalli e trend non costituiscono sempre una decisione SPRT.

| Cambiamento | Evidenza storica | Decisione ragionevole oggi |
|---|---|---|
| Ampliare RFP in depth | Score 37,03%, LOS 0% | Non ripetere lo stesso gate |
| Node-effort time management | H0, −4,0 ±5,2 a 8682 partite | Una nuova proposta deve cambiare segnale/modello, non solo costante |
| IIR >=6→>=4 | +1,37 ±3,37 a 20k | Non è un guadagno dimostrato; bassa priorità |
| LMR indice 4→3 / depth 3→2 | Trend −1,80 ±6,89 / −6,84 ±11,89 | Non ripetere indiscriminatamente |
| Rimuovere cap delle check extension | −10,28 ±14,33 | Non estendere tutti gli scacchi |
| Divisore quiet history 8192→4096 | +1,48 ±4,85; +19,7% nodi | Nessun beneficio convincente |
| Aspiration window ×2,6 | Meno retry ma +71% nodi; −7,72 ±10,78, interrotto | Retry rate da solo è la metrica sbagliata |
| Hash 16/8 MiB | −1,44 ±3,80 / −1,97 ±9,62 | Non ridurre memoria senza evidenza nuova |
| PGO | NPS −4% e −9% | Evitare il terzo tentativo identico |
| Razoring ripristinato | +1,1 ±6,4 | Bassa priorità |
| Continuation history multipla per ordering | Circa −7 Elo | Non aggiungere array solo perché usati altrove |
| Più dati della stessa distribuzione | −7,30 ±8,61 | Migliorare target/copertura misurata prima del volume |
| Più training della stessa famiglia | Incrementi decrescenti +29,4→+9,9→+2,7 | Servono validation e nuova ipotesi |
| SEE attack gating | Variante PEXT neutra; altra variante −2,66% NPS | Non assumere che evitare lookup sia sempre più veloce |
| Rimuovere tutta la quiet SEE | Più nodi e tempo peggiore | Conservare la variante pawn-only già adottata come baseline |
| Store di mosse in qsearch TT | Forte perdita documentata | Tenere separati ordering e bound-only store |
| Tagliare righe morte contHist | Già fatto: 784→576 KiB, NPS neutro | Non contarne di nuovo il beneficio |
| MAX_QSEARCH_DEPTH 48 assoluto | Zero interventi in 36,5 milioni di chiamate osservate | P3; non rimuovere il backstop sulla base di un campione |
| fullMoveClock u8 oltre 255 | L'orizzonte TM è già al minimo da molto prima | Nessuna priorità Elo in questo uso |

Sono già implementati: TT cache line e prefetch principali, raw eval TT, lazy SEE principale, move storage non inizializzato oltre il prefisso live, king mirror buckets/Finny, update from/to fuso, pairwise SIMD esatto, L1 VNNI sparso e riordino, LMR precomputata, legal evasions specializzate, diversificazione e voto SMP. Rimuovere allocazioni inesistenti, proporre magics al posto dei PEXT già usati o promettere un vantaggio dalla sola dimensione degli array non è un intervento fondato.

## 14. Sequenza pratica: cosa cambiare, togliere e aggiungere

La prima fase parte **dopo la conclusione o sospensione decisa dall'utente dello SPRT attivo**. Nessuna delle azioni seguenti è stata eseguita in questa sessione.

### Patch circoscritte

1. S01: rebase TT prima dei confronti, in principale e qsearch; correggere anche il commento del contratto.
2. S03 e S04, in cambi distinti: cutoff root anche sulla prima mossa; propagazione interruption prima di store/apprendimento/re-search.
3. S02: stack di eval esplicito per worker, root inizializzata, slot validi e protezione dei probe stesso-ply.
4. S05: identità completa della mossa esclusa e validazione hash prima dei return SE; separare dalle scelte di pruning del nodo excluded.
5. I01/I05: configurazione Hash e artefatti/configurazioni di build affidabili per le campagne successive.
6. N10/M02/M03b/B05/B07: reload/reset/EP/TB e precedenza del risultato TB corretti con piccoli test mirati; non riservare a bug freddi la campagna Elo più lunga.
7. S11 percorso sequenziale delle evasioni qsearch; N08 hint deep e N09 accesso ai quattro byte con contratto valido. Misure di costo solo dove rilevanti.

### Refactor intermedi

8. N02: delta finali per catture/promozioni e kernel fusi, preservando tutte le eval e i nodi nei confronti deterministici.
9. S08: ingresso orizzonte unificato, con contabilità corretta; S13: SEE a soglia e riuso locale, senza cache hash grande.
10. S09: ProbCut ordinato, poi singole ablation di pretest/gate/soglia. Non combinare sette cambi prima del primo confronto.
11. C01–C06: versione correction history con baseline e bound espliciti, **solo dopo il verdetto del test corrente**; ablation pawn-only vs tre segnali.
12. T01/T02 e M01: conservazione raw eval/entry profonde e pool helper, separatamente; valore da misurare ai TC e Threads reali.

### Investimenti strutturali

13. N01: accumulator stack realmente lazy, riutilizzando i delta già collaudati; è il primo grande refactor NNUE proposto.
14. S10: staging completo, soltanto se il nuovo profilo mostra abbastanza generazione/scoring evitabile.
15. N05/N06: campionamento sui veri forward e riordino/AVX2 sparso; lasciare indipendenti le varianti CPU.
16. D01–D03: validation esterna, analisi residui, relabel mirato e quantizzazione consapevole. Solo allora D04, una architettura alla volta.

Le dipendenze contano: cambiare rete sposta la distribuzione di sparsità e i margini di pruning; cambiare search sposta i dati utili per reorder/training; lo stack lazy cambia il valore relativo dei TT eval hit. Dopo una modifica strutturale rifare soltanto le misure rese obsolete, senza pretendere che tutti i guadagni individuali si sommino.

## 15. Suggerimenti di patch concreti, non applicati

Per S01, il cambiamento richiesto è questo schema; mantenere i gate depth/PV/excluded del singolo chiamante:

```cpp
if (tte.hit) {
    const int32_t ttScore = scoreFromTT(tte.score, ply);
    if (/* gate del chiamante */ &&
        ttBoundCutoff(tte.flag, ttScore, alpha, beta)) {
        return ttScore;
    }
}
```

Per S03, dopo l'aggiornamento della miglior mossa alla radice:

```cpp
if (isBetaCutoff(bestScore, beta)) break;
```

Per S04 la regola è «ripristinare lo stato, poi controllare l'interruzione, poi usare lo score». Non basta impedire lo store finale della root: vanno protetti qsearch e probe interni. Un tipo risultato `score + completed` potrebbe rendere il contratto esplicito, ma cambiare tutta l'ABI della ricorsione non è necessario per la prima patch.

Per C02 mantenere separati i valori e determinare il bound usando alpha originale:

```text
rawEval       := NNUE oppure raw eval TT
correctedEval := correzione coerente di rawEval
searchEstimate := eventuale TT bound più informativo per pruning

learn := exact
      oppure (lower e score > correctedEval)
      oppure (upper e score < correctedEval)
```

Per N02 il punto di aggancio è la descrizione **finale** della mossa: non chiamare prima tutti gli hook eager e poi il kernel fuso. Per N01 il piccolo delta può stare nello stato per ply; i buffer da 4096 B appartengono al worker e sono materializzati quando servono. La Board usata fuori dalla search deve mantenere un percorso di eval valido senza puntatori a stack già distrutti.

## 16. Profiling e validazione, da eseguire a CPU libera

### Piano minimo di misura

1. **Snapshot:** binari/reti/configurazioni immutabili; una sola variante per volta. Usare la stessa ISA e una CPU/core class coerente. Non ricompilare o profilare i processi dello SPRT attivo.
2. **Correttezza mirata:** eseguire solo i test del contratto cambiato, poi la suite necessaria alla superficie interessata. Una patch docs non richiede perft; una patch Board/FT sì.
3. **Profilo baseline:** corpus fisso di aperture, mediogiochi tattici/chiusi, finali, EP/promozioni/arrocco e posizioni con storia. Prima Threads=1, poi configurazione SMP reale. Profilo ottimizzato con simboli; separare self e inclusive.
4. **Contatori hardware:** cycles, instructions, branch misses, L1/LLC misses, dTLB e frontend/backend stalls quando disponibili. Controllare perf event support e mapping huge page reale. `perf stat`/sampling di una copia dedicata, mai attach al test in corso.
5. **Contatori applicativi:** privati per worker, attivati a compile time e raccolti alla fine. Niente atomiche/log per nodo. Campionare i forward per densità NNZ invece di riversare ogni FEN su disco.
6. **Confronto velocità:** medesimo carico, warm-up separato, A/B alternati e più ripetizioni sufficienti a stimare dispersione. Misurare tempo totale/posizione e tempo per depth completata; NPS soltanto se il contatore ha identica semantica. Fermare l'ampliamento dei test quando il risultato è chiaro.
7. **Forza:** per cambi che alterano l'albero/rete, match a tempo fisso con aperture accoppiate e colori invertiti, ipotesi SPRT definite prima. Conferma a TC più lungo per vincitori che cambiano pruning/tempo/architettura. Un test troncato per trend resta inconclusivo; non accumulare solo i checkpoint fortunati.

### Metriche che decidono i singoli interventi

| Famiglia | Verifica di correttezza | Misura di costo/beneficio | Criterio di accettazione |
|---|---|---|---|
| TT/root/stop | Finestra valida, mate rebase, abort senza bound, ultima depth completa | Decisioni TT/root cambiate, re-search, stop latency | Contratto corretto; capire eventuali regressioni senza conservare il bug |
| Lazy/fused FT | Accumulatori vs rebuild dopo ogni transizione mirata, eval scalare/SIMD | Aggiornamenti evitati, byte logici, refresh/diff, wall time search | Eval identiche e risparmio nell'intera ricerca su corpus rappresentativo |
| NNUE SIMD/reorder | Stessa funzione per tutti i bucket, soglie e casi densi/sparsi | ns/forward, gruppi attivi, spill, cache, tempo search | Guadagno sui target pertinenti, nessuna divergenza non spiegata |
| Staging/SEE/ProbCut | Legalità, promozioni, pin/EP, no falso bound contrattuale | Mosse generate/scorate/visitate, SEE/cutoff per rango, nodi dei tentativi falliti | Migliore forza a tempo fisso; NPS o meno nodi da soli non bastano |
| Correction/history | Baseline, flag, reset e cap verificati | Errore futuro holdout, update utili, costo lookup, decisioni cambiate | SPRT favorevole dopo ablation; non solo distribuzione più ampia dei valori |
| TT replacement/cache eval | Consistenza entry e score, stress concorrente dedicato | Hit eval/bound per depth/age, store, traffico e forza | Risparmio netto senza perdita d'informazione utile |
| Board/movegen | Perft e sequenze make/undo/hash/repetition; casi null e EP | Costo make/undo e movegen nel search | Risultato corretto e vantaggio totale, non solo microbench |
| SMP/tempo/UCI | Stop/start/reset/ponderhit, risultato coerente, budget | p95/p99 latenza, scaling Threads, depth completate, timeout | Più forza al TC reale e nessuna regressione del protocollo |
| Training/architettura | Export compatibile e confronto oracle | Holdout/calibrazione per fase, errore quantizzato, tempo search | Checkpoint scelto senza leakage e conferma Elo a pari tempo |

Test deterministici prioritari da aggiungere quando si implementa: TT mate lower/upper ai diversi ply; fail-high sulla prima root move; ply-2 improving con eval root positiva e negativa; SE con quattro promozioni sulla stessa coppia di case; stop durante qsearch/SE/ProbCut; re che cambia base seguito da evaluate/undo e visita del sibling; null seguita da mosse reali e undo; EP unica mossa; matto a clock 100; Syzygy con diritti di arrocco; reload A→B→C con cache popolata. Per i refactor di stato, sanitizer e stress SMP in sessioni dedicate, senza interpretarne la velocità come quella del binario competitivo.

## 17. Copertura e limiti dell'audit

Lettura statica focalizzata su `engine/search`, `engine/sort`, `engine/evaluator`, `nnue` runtime/accumulatori/loader/kernel/selftest, `board` do/undo/hash/ripetizioni/attacchi, `tt`, `engine/time`, `engine/engine`, UCI, wrapper Syzygy, datagen/trainer/export/reorder/shuffle/coverage e makefile/harness. Driver, FEN, perft e test sono stati esaminati per collegamento al percorso e affidabilità dei confronti. Le dipendenze vendorizzate Boost.UT e il codice interno Pyrrhic non sono stati sottoposti a una revisione riga per riga: non è emersa evidenza per farne una priorità prestazionale; il wrapper e i suoi contratti sono inclusi.

I profili e gli Elo citati sono storici; nessun campione raccolto oggi misura il costo della variante locale corrHist o della rete su questa CPU. I casi FEN proposti sono costruiti dalla lettura del codice e restano da eseguire. L'audit produce **questo solo documento**: nessuna patch motore, build, partita, training, benchmark o modifica al test in background.
