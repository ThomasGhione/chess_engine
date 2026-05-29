# HydraY — Bug Audit

Audit di tutto il codice in scope: `board/`, `tt/`, `engine/` (search, eval, time, opening), `uci/`, `driver/`. Esclusi: `engine/syzygy/` (Pyrrhic vendored), `engine/opening/polyglot_keys.hpp` (tabella generata), `board/magic_numbers.hpp` (tabella).

Severità:
- **CRITICO** — UB / dati corrotti / perdita di correttezza
- **ALTO** — bug funzionale grave o perdita di forza significativa
- **MEDIO** — bug minore / scelta sbagliata in alcuni nodi
- **BASSO** — code smell / fragilità che può diventare bug

---

## MEDIO

### M1 — `safeParseInt` ritorna `uint8_t` e clampa silenziosamente
- **File**: [board/fen.cpp:44-49](board/fen.cpp#L44-L49)
- **Cosa**: FEN con `fullmove` > 255 viene saturato a 255 senza errore. `halfmove` similmente in [board.cpp:90-92](board/board.cpp#L90-L92).
- **Impatto**: posizioni di analisi/test con full-move count alto perdono informazione.
- **Fix**: allargare `halfMoveClock`/`fullMoveClock` a `uint16_t` (il bilancio sizeof Board cambia di 2 byte).

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
| MEDIO | 3 (M1, M3, M4) |
| BASSO | 12 (B1–B12) |

## Note

I bug ad alta priorità (CRITICO e ALTO) sono tutti risolti. Quello che resta è
polish: M1 saturazione `uint8` per partite >255 fullmoves (rarissime), M3/M4
hardening del threading senza bug osservato, e i BASSO sono code smell.

## Aree non analizzate

- `engine/syzygy/` (Pyrrhic vendored — eccezione ragionevole)
- `engine/opening/polyglot_keys.hpp` (tabella 533 righe di costanti)
- `board/magic_numbers.hpp` (tabella magic numbers)
- `tests/`, `tuning/`, `script/`, `output/`, file di build
