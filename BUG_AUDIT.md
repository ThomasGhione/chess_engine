# HydraY — Bug Audit

Audit di tutto il codice in scope: `board/`, `tt/`, `engine/` (search, eval, time, opening), `uci/`, `driver/`. Esclusi: `engine/syzygy/` (Pyrrhic vendored), `engine/opening/polyglot_keys.hpp` (tabella generata), `board/magic_numbers.hpp` (tabella).

Severità:
- **CRITICO** — UB / dati corrotti / perdita di correttezza
- **ALTO** — bug funzionale grave o perdita di forza significativa
- **MEDIO** — bug minore / scelta sbagliata in alcuni nodi
- **BASSO** — code smell / fragilità che può diventare bug

---

## ALTO

### A1 — `evalStack[0]` mai inizializzato → euristica `improving` falsata a ply 2
- **File**: [engine/search/searcher.cpp:906-918](engine/search/searcher.cpp#L906-L918)
- **Cosa**: `static thread_local int32_t evalStack[MAX_PLY] = {}`. La scrittura avviene solo `if (ply > 0 && ply < MAX_PLY)`. Quindi `evalStack[0]` resta `0` per tutta la vita del thread (nessuno la scrive mai). A ply==2:
  ```cpp
  const bool improving = !node.inCheck && ply >= 2
      && evalStack[ply - 2] != NEG_INF       // sempre vero (0 != NEG_INF)
      && (node.staticEval > evalStack[ply - 2]);  // diventa staticEval > 0
  ```
- **Impatto**: a ply 2 `improving` è sistematicamente `staticEval > 0`. Bias persistente su futility threshold, LMP threshold, RFP.
- **Fix**: nel root inizializzare `evalStack[0]`:
  ```cpp
  if (ply == 0) {
      const int32_t rootStaticEval = node.inCheck ? NEG_INF : Evaluator::evaluate(b);
      evalStack[0] = rootStaticEval;
  }
  ```
  Va calcolato anche al root (oggi al root `node.staticEval` non viene mai computato perché la guardia è `ply > 0`).

### A2 — `generateTacticalMoves`: cattura del re senza check di legalità → mosse illegali in Probcut
- **File**: [engine/search/move_generator.cpp:399-402](engine/search/move_generator.cpp#L399-L402), uso in [engine/search/searcher.cpp:1017-1029](engine/search/searcher.cpp#L1017-L1029)
- **Cosa**: il loop king-attacks aggiunge `KING_ATTACKS[kingIndex] & oppOcc` senza `isLegalPseudoMove`. La qsearch ha un filtro esplicito per re ([searcher.cpp:1214-1220](engine/search/searcher.cpp#L1214-L1220)), ma il loop Probcut **no**.
- **Impatto**:
  1. Spreca lavoro su posizioni illegali (re lasciato in scacco).
  2. Il sottoramo del figlio computa score mate-like derivato dalla cattura di re, e quegli score possono entrare in TT (`allowTTWrite` viene ereditato dall'esterno nella chiamata probcut). Contaminazione TT.
- **Fix**: nel loop probcut, filtrare:
  ```cpp
  const int fromPieceType = b.get(mc.from.index) & Board::MASK_PIECE_TYPE;
  if (fromPieceType == Board::KING && !b.isLegalPseudoMove(mc.from.index, mc.to.index, b.get(mc.from.index))) continue;
  ```
  In alternativa filtrare a monte in `generateTacticalMovesFor` (più pulito, costo costante).

### A3 — `setoption` durante search → race su `engine::*` globali (e su `Board::MATERIAL_VALUES`)
- **File**: [uci/uci.cpp:392-496](uci/uci.cpp#L392), confronto con [uci/uci.cpp:498-525](uci/uci.cpp#L498) (position) e [uci/uci.cpp:537](uci/uci.cpp#L537) (go)
- **Cosa**: `position` e `go` chiamano `finishSearch(true, false)` per fermare il thread prima di modificare lo stato; `setOption` **non lo fa**. I valori `engine::PAWN_VALUE`, `engine::*_PENALTY`, `PIECE_VALUES[]`, `MVV_TABLE[]`, `Board::MATERIAL_VALUES[]` sono `int32_t` non-atomici, modificati live mentre il search thread legge.
- **Impatto**: race su int32 → tearing improbabile su x86 ma comportamento indefinito formale; eval inconsistente per centinaia di nodi.
- **Sotto-bug correlato** (era A3 originale): `refreshPieceTables()` aggiorna `Board::MATERIAL_VALUES` ma `incrementalMaterialDelta` accumulato sul Board attuale resta calcolato sui vecchi valori. Mismatch tra delta e tabella corrente finché `position` non re-parsa il FEN (che rifà `updateOccupancyBB()`).
- **Fix**:
  1. In `setOption`, prima di qualunque scrittura: `finishSearch(true, false);`.
  2. Dopo `refreshPieceTables()`: chiamare `engine.board.updateOccupancyBB()` per ricostruire i delta incrementali coerenti con la nuova tabella.

### A4 — Polyglot side-to-move XOR: chiave potrebbe non matchare i .bin polyglot standard
- **File**: [engine/opening/opening_book.cpp:117-121](engine/opening/opening_book.cpp#L117-L121)
- **Cosa**: lo standard polyglot prevede `XOR Random64[780]` quando WHITE è a muovere. Il codice fa `key ^= POLYGLOT_KEYS[0]` per WHITE, con commento "cutechess convention, equivalent". Funziona solo se `POLYGLOT_KEYS[]` è stato generato in modo che `[0]` sia la chiave side-to-move.
- **Impatto**: se il file caricato è polyglot standard ma la tabella interna è cutechess-style (o viceversa), nessuna entry matcha mai e il book è muto.
- **Fix**: verificare empiricamente che `probe(startpos)` con `komodo.bin` ritorni una mossa non-nullopt. Se no, allineare l'offset o la tabella allo standard polyglot (`Random64[780]`).

---

## MEDIO

### M1 — `safeParseInt` ritorna `uint8_t` e clampa silenziosamente
- **File**: [board/fen.cpp:44-49](board/fen.cpp#L44-L49)
- **Cosa**: FEN con `fullmove` > 255 viene saturato a 255 senza errore. `halfmove` similmente in [board.cpp:90-92](board/board.cpp#L90-L92).
- **Impatto**: posizioni di analisi/test con full-move count alto perdono informazione.
- **Fix**: allargare `halfMoveClock`/`fullMoveClock` a `uint16_t` (il bilancio sizeof Board cambia di 2 byte).

### M2 — `Engine::reset()` non ferma il watchdog del TimeManager
- **File**: [engine/engine.cpp:123-144](engine/engine.cpp#L123-L144)
- **Cosa**: il `timeManager` non viene fermato. Se `reset` arriva durante una search timed, il watchdog può scattare *dopo* il reset, settando `stopSearchRequested` su una search successiva.
- **Fix**: in `reset()` aggiungere `this->timeManager.stop();` prima della logica di reset.

### M3 — `TimeManager::stop()` non perfettamente idempotente sotto race
- **File**: [engine/time/time_manager.cpp:155-165](engine/time/time_manager.cpp#L155-L165)
- **Cosa**: il primo lock check `if (!running_ && !watchdog_.joinable()) return` poi sblocca, segnala, ri-locka per `running_=false`. Due chiamate concorrenti possono entrambe procedere oltre la prima guardia e una delle due fare `join()` su un thread già joinato.
- **Impatto**: in pratica protetto dal `searchApiMutex` esterno; bug latente se mai si rimuove la protezione.
- **Fix**: tenere `mtx_` per tutta la durata o usare `std::call_once` / flag atomico.

### M4 — `Engine::ponderRootHash` / `ponderResultReady` letti/scritti senza atomic
- **File**: [engine/engine.cpp:138-142](engine/engine.cpp#L138-L142), [engine/engine.cpp:271-274](engine/engine.cpp#L271-L274), [engine/engine.cpp:304-306](engine/engine.cpp#L304-L306)
- **Cosa**: scritti dal worker pondering, letti dal main thread in `tryUsePonderResult`. Sincronizzati solo tramite il `join()` in `stopPondering()` (happens-before via thread completion).
- **Impatto**: in pratica safe perché `tryUsePonderResult` viene chiamato dopo `stopPondering`. Se mai il flusso cambia, race silenziosa.
- **Fix**: rendere `ponderResultReady`, `ponderResultDepth`, `ponderResultScore`, `ponderResultMove`, `ponderRootHash` atomici o proteggerli con `ponderingMutex`.

### M5 — `Engine` hard-codes path "engine/komodo.bin" relativo
- **File**: [engine/engine.cpp:108](engine/engine.cpp#L108)
- **Cosa**: se l'eseguibile è lanciato da una working directory diversa dalla root del repo, il book non si carica e l'errore non è visibile (silenzioso fino a `info string BookFile` via UCI).
- **Fix**: o risolvere il path rispetto a `argv[0]` / `std::filesystem::current_path()`, o emettere `info string BookFile error: ...` quando il load iniziale fallisce.

### M6 — `Driver::saveGame()` non controlla errori I/O
- **File**: [driver/driver.cpp:217-222](driver/driver.cpp#L217-L222)
- **Cosa**: `std::ofstream saveFile("saves/save.txt")` senza check su `.is_open()` né su `saveFile.good()` dopo write. Errori silenziosi.
- **Fix**: check `if (!saveFile) { std::cerr << "Error: cannot write save file\n"; return; }`.

### M7 — `MoveList::moveFrom` per trivially-copyable non distrugge i sorgenti
- **File**: [engine/movelist.hpp:96-105](engine/movelist.hpp#L96-L105)
- **Cosa**: per trivially-copyable T, `moveFrom` fa memcpy e setta `other.size = 0`. I "vecchi" oggetti restano nella storage sorgente ma non sono più referenziabili.
- **Impatto**: per `Board::Move` (trivialmente distruttibile) è innocuo. Se T cambia tipo in futuro, potenziale leak.
- **Fix**: documentare la precondizione o aggiungere `static_assert(std::is_trivially_destructible_v<T>)`.

### M8 — `std::aligned_storage_t` deprecato in C++23
- **File**: [engine/movelist.hpp:13](engine/movelist.hpp#L13)
- **Cosa**: `using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;` deprecato in C++23. Genera warning con `-std=c++23 -Wdeprecated`.
- **Fix**:
  ```cpp
  struct alignas(T) Storage { std::byte data[sizeof(T)]; };
  ```

### M9 — `Searcher::shouldDeltaPrune` vs `Sorter::shouldDeltaPrune` semantica diversa
- **File**: [engine/search/searcher.hpp:197](engine/search/searcher.hpp#L197), [engine/search/sorter.cpp:417-419](engine/search/sorter.cpp#L417-L419)
- **Cosa**: `Searcher::shouldDeltaPrune` usa `<=` (`standPat + margin <= alpha`); `Sorter::shouldDeltaPrune` usa `<` (`standPat + margin < alpha`). Naming identico, semantica diversa.
- **Impatto**: confusione, possibile bug futuro per chi rinomina/refactora.
- **Fix**: dare nomi distinti (`deltaPruneStrict` / `deltaPruneInclusive`) o uniformare.

### M10 — `runIterativeDeepening`: cast `runtime.depth - 1` su `uint64`
- **File**: [engine/search/searcher.cpp:328-330](engine/search/searcher.cpp#L328-L330)
- **Cosa**: `runtime.depth` è `uint64_t`. `runtime.depth - 1` con depth=0 underflows a `UINT64_MAX`, poi cast a `int32_t` → `-1` → quiescenza. Il flusso normale ha `runtime.depth >= 1` garantito da `firstDepth = max(1, startDepth)`, ma è fragile.
- **Fix**: `if (runtime.depth == 0) return /* qsearch */;` o assertion.

---

## BASSO

### B1 — `Board::repetitionHistory` bloata sizeof(Board)
- **File**: [board/board.hpp:341](board/board.hpp#L341)
- **Cosa**: `std::array<uint64_t, 101>` = 808 byte per ogni Board. `searchDeferredRootMove` in YBWC copia un Board per worker per move → ~1KB di memcpy a copia.
- **Fix**: separare la history in struttura allocata a parte o usare `std::deque` con capacità minima.

### B2 — `Driver::clearScreen` usa `std::system("clear"/"cls")`
- **File**: [driver/driver.cpp:448-455](driver/driver.cpp#L448-L455)
- **Cosa**: chiamata a shell esterna. Già annotata `MIGHT NOT BE NOEXCEPT` nel codice.
- **Fix**: usare escape ANSI direttamente (`std::cout << "\033[2J\033[H"`).

### B3 — `LMRTable` constexpr usa `__builtin_log`
- **File**: [engine/search/searcher.cpp:33-34](engine/search/searcher.cpp#L33-L34)
- **Cosa**: `__builtin_log` è estensione GCC. Clang lo accetta in constexpr ma non è portatile a MSVC.
- **Fix**: precomputare la tabella offline o usare approssimazione constexpr-pure.

### B4 — `Coords::INVALID_COORDS = 255` vs check `index < 64`
- **File**: [board/coords.hpp:14,57](board/coords.hpp#L14)
- **Cosa**: `INVALID_COORDS = 255` ma `isValid() = index < 64`. Qualsiasi valore 64..254 è "invalido" senza essere il sentinel canonico.
- **Fix**: o forzare `index = INVALID_COORDS` in ogni costruzione fallita, o cambiare `isValid` a `index != INVALID_COORDS`.

### B5 — `Engine` ctor `MAX_THREADS(searchRuntime.maxThreads)` bind reference prima del body
- **File**: [engine/engine.cpp:96-110](engine/engine.cpp#L96-L110)
- **Cosa**: in initializer list `MAX_THREADS` è bound a `searchRuntime.maxThreads` *prima* che il body lo setti a `omp_get_max_threads()`. Funziona perché è reference, ma è confusionario.
- **Fix**: spostare l'inizializzazione di `searchRuntime.maxThreads` in initializer list o documentare.

### B6 — `Sorter::staticExchangeEvaluation` cache thread_local ~1.1 MiB per thread
- **File**: [engine/search/sorter.cpp:164-166](engine/search/sorter.cpp#L164-L166)
- **Cosa**: `SEE_CACHE_SIZE = 65536`, `sizeof(SEECacheEntry) ≈ 17`. Costo significativo con molti thread Lazy SMP.
- **Fix**: ridurre size o usare cache condivisa con seqlock.

### B7 — `Engine::moveHistory` cap a 6144 byte
- **File**: [engine/engine.hpp:95-97](engine/engine.hpp#L95-L97)
- **Cosa**: dopo 1024 mosse il prefisso viene troncato. Per partite analisi lunghe l'history è persa.
- **Fix**: opzionale, è scelta di design.

### B8 — `std::isspace` / `std::tolower` con `char` raw → potenziale UB su char con segno
- **File**: vari ([uci.cpp:18-23](uci/uci.cpp#L18), [fen.cpp:22](board/fen.cpp#L22), `coords.hpp`, `driver.cpp`)
- **Cosa**: la maggior parte dei call site fa già `static_cast<unsigned char>(c)` (corretto). Ma `coords.hpp:71` (`isLetter`/`isNumber`) compara `char` con literals — OK per ASCII ma fragile su EBCDIC.
- **Fix**: nessuna azione necessaria per il flusso UCI/FEN; documentazione.

### B9 — `MovePickerData` size ~3.5KB sullo stack
- **File**: [engine/search/sorter.hpp:63-119](engine/search/sorter.hpp#L63), [engine/search/searcher.cpp:1057](engine/search/searcher.cpp#L1057)
- **Cosa**: 218 entries * (sizeof(Move)+int32+int8). RVO attesa, ma con compilatore non-cooperativo potrebbe scattare copia.
- **Fix**: verificare con `-fdump-tree-optimized` o forzare NRVO con struttura return-by-out-param.

### B10 — `Sorter::sameFromTo` con killer "vuoto" potrebbe matchare se index sentinel coincide
- **File**: [engine/search/sorter.cpp:22-28](engine/search/sorter.cpp#L22)
- **Cosa**: killer default `Move{}` ha `from.index = to.index = 255`. Nessuna mossa legale ha index 255, quindi match impossibile. **Già safe**, ma fragile se il sentinel cambia.
- **Fix**: aggiungere `if (killer.from.index >= 64) return false;` come early-out esplicito.

### B11 — Ridondanza inutile `Coords::isInBounds(c)` vs `c.isValid()`
- **File**: [board/coords.hpp:57,72](board/coords.hpp#L57)
- **Cosa**: `isInBounds(c) → c.isValid() → index < 64`. Due API per la stessa cosa.
- **Fix**: deprecare `isInBounds`, usare solo `isValid`.

### B12 — `repetitionHistory` memmove quando full: O(n) per move
- **File**: [board/board.cpp:366-370](board/board.cpp#L366-L370)
- **Cosa**: a partita lunga (>101 mezze-mosse reversibili) ogni doMove fa memmove di 800 byte. Trascurabile rispetto al resto del move, ma circular buffer sarebbe O(1).
- **Fix**: opzionale (premature optimization).

---

## Tabella riepilogo

| Severità | # voci |
|---|---|
| ALTO | 4 (A1, A2, A3, A4) |
| MEDIO | 10 (M1–M10) |
| BASSO | 12 (B1–B12) |

## Top da fixare per impatto/effort

1. **A1** (`evalStack[0]`) — 1 riga al root, possibile guadagno ELO misurabile.
2. **A2** (Probcut king-capture) — 3 righe nel loop probcut, evita contaminazione TT.
3. **A3** (setoption race + MATERIAL_VALUES stale) — 2 righe in `setOption`, robustezza.
4. **A4** (polyglot side-to-move XOR) — da verificare empiricamente con `komodo.bin`.

## Aree non analizzate

- `engine/syzygy/` (Pyrrhic vendored — eccezione ragionevole)
- `engine/opening/polyglot_keys.hpp` (tabella 533 righe di costanti)
- `board/magic_numbers.hpp` (tabella magic numbers)
- `tests/`, `tuning/`, `script/`, `output/`, file di build
