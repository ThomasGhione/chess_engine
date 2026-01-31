# Patta per tripla ripetizione — Design

## Obiettivo e vincoli
- Implementare il riconoscimento automatico della tripla ripetizione (patta immediata, senza necessità di claim).
- Riutilizzare le strutture esistenti (Board, Zobrist) minimizzando l’impatto sulle performance dei path critici di mosse.
- Limitare memoria e complessità: cronologia massima vincolata dalla regola delle 50 mosse (100 half-move).
- Garantire coerenza con undo/redo (doMove/undoMove, moveBB) e con l’aggiornamento di en passant/castling.
Fonti: `board/board.hpp` linee 558-565; `board/board.cpp` linee 159-176 (halfMoveClock reset e avanzamento colore); `tt/zobrist.hpp` linee 31-116 (hash di posizione).

## Stato attuale
- La patta è rilevata solo per stallo e regola delle 50 mosse; la tripla ripetizione non è implementata (commento esplicito).
- halfMoveClock viene azzerato su catture o mosse di pedone e incrementato altrimenti, quindi fornisce il bound naturale per le posizioni rilevanti.
- Zobrist hashing è disponibile e già usato per la TT (`zobrist::computeHashKey(board)`), ma la chiave non è mantenuta incrementalmente nel Board.
Fonti: `board/board.hpp` linee 558-565; `board/board.cpp` linee 159-176; `board/board.hpp` linea 562 (nota mancanza tripla ripetizione); `tt/zobrist.hpp` linee 31-116.

## Analisi dell’idea proposta (array statico di 50 coppie <string,bool>)
- **Pro**: semplice, spazio O(50), nessuna struttura dinamica.
- **Contro**:
  - Uso di stringhe per l’hash è costoso; un 64-bit Zobrist è sufficiente.
  - Il flag bool “già visto” per posizione non gestisce bene più di due occorrenze sparse (può perdere il conteggio reale se la posizione ricorre più volte intervallata).
  - Manca integrazione con undo (necessaria per ricerca o per annullare mosse utente).
  - Lineare O(50) su ogni mossa; accettabile, ma il bool non riduce realmente il lavoro.
Fonti: Idea utente (prompt); `board/board.hpp` linea 562 (funzionalità mancante).

## Soluzione raccomandata
### Dati mantenuti
- `uint64_t currentHash;` nel Board (hash Zobrist della posizione corrente).
- `std::array<uint64_t, 128> repetitionHistory; uint8_t historySize;` come buffer circolare limitato alle posizioni dall’ultima mossa irreversibile (<=100 half-move ⇒ 128 è sufficiente).
- Facoltativo per debug: `uint8_t repetitionCount[128];` oppure contatore calcolato al volo (preferito: conteggio al volo per non duplicare stato).

### Aggiornamento hash (incrementale)
- In `doMove/moveBB`: aggiornare `currentHash` con XOR dei contributi coinvolti (pezzo mosso, catturato, en passant, castling, side to move). È più leggero che ricalcolare da zero.
- In `undoMove`: invertire le stesse XOR per ripristinare `currentHash`.
- All’inizializzazione o parsing FEN: calcolare `currentHash = zobrist::computeHashKey(*this);`.
Fonti: `board/board.cpp` linee 9-178 (flusso make move); `board/boardenginemove.cpp` linee 13-198 (doMove); `tt/zobrist.hpp` linee 69-116 (componenti dell’hash).

### Gestione cronologia
- Dopo ogni mossa legale, push di `currentHash` nel buffer; pop (o reset historySize=1 con sola posizione corrente) quando halfMoveClock viene azzerato (cattura o mossa di pedone). Questo mantiene solo le posizioni legalmente rilevanti per la tripla ripetizione.
- In `undoMove`: decrementare `historySize` e ripristinare `currentHash` da `repetitionHistory[historySize-1]`.
- Su creazione Board (FEN o start): `repetitionHistory[0] = currentHash; historySize = 1;`.
Fonti: `board/board.cpp` linee 25-176 (en passant, castling, halfMoveClock); `board/boardenginemove.cpp` linee 13-198 (doMove); `board/boardenginemove.cpp` linee 200-262 (undoMove).

### Rilevazione tripla ripetizione
- Funzione `bool Board::isThreefoldRepetition() const`:
  - Scansiona a ritroso `repetitionHistory[0..historySize-1]` contando le occorrenze di `currentHash`.
  - Se conteggio ≥ 3, restituisce true.
  - Complessità O(N) con N ≤ 100; costo trascurabile rispetto al resto del move making.
- Variante opzionale (se servisse più velocità): piccolo open-addressing table su 128 slot con contatore, mantenuta in parallelo; non necessaria in prima iterazione.

### API e integrazione
- Estendere `isDraw` a includere `isThreefoldRepetition()` (oltre a stallo e 50-move).
- Esporre `isThreefoldRepetition()` per eventuali usi GUI/engine; mantenere semantica di patta automatica nel `updateGameResult`.
- Assicurare copertura sia per `moveBB` (path ad alto livello) sia per `doMove/undoMove` (path engine/search); entrambi devono aggiornare hash e cronologia.

### Edge case gestiti
- En passant e castling influenzano l’hash (già previsto in Zobrist); l’aggiornamento incrementale deve XOR-are i contributi di en passant e castling rights prima/dopo la mossa.
- Promozioni: sostituire la piece-key corretta (rimuovere pedone, aggiungere pezzo promosso) nell’hash incrementale.
- Mosse illegali non entrano in cronologia perché `moveBB` ritorna false prima del push.
Fonti: `tt/zobrist.hpp` linee 99-114 (side-to-move, castling, en-passant nell’hash); `board/board.cpp` linee 149-176 (en passant, clock); `board/boardenginemove.cpp` linee 167-185 (promozioni).

## Piano di implementazione
1. Aggiungere campi `currentHash`, `repetitionHistory`, `historySize` al Board e inizializzarli in costruttori/FEN (`fromFenToBoard`).
2. Integrare aggiornamento incrementale hash in `moveBB` e `doMove/undoMove`; fallback temporaneo: ricalcolo completo con `zobrist::computeHashKey` finché non si ottimizza l’incrementale.
3. Gestire reset della cronologia quando `halfMoveClock` viene azzerato (cattura/pedone) e push dopo ogni mossa valida.
4. Implementare `isThreefoldRepetition()` e usare il risultato in `isDraw` e in `engine::updateGameResult`.
5. Aggiungere test/fixture (per quanto disponibili) con sequenze note di ripetizione e con mosse irreversibili che resettano la storia.
Fonti: `engine/evaluate.cpp` linee 1203-1215 (updateGameResult usa isDraw); `board/board.hpp` linee 558-565 (isDraw corrente); `tt/zobrist.hpp` linee 69-116 (hash completo).

## Rischi e aperti
- Collisioni Zobrist sono estremamente improbabili ma possibili; accettabili per motore amatoriale.
- Necessario mantenere coerenza tra i due percorsi di move making (`moveBB` e `doMove`); differenze attuali possono causare divergenza se uno dei due dimentica di aggiornare la storia.
- Prestazioni: scansione lineare su ≤100 elementi è trascurabile; l’aggiornamento incrementale dell’hash elimina la necessità di ricostruire bitboard.
Fonti: `board/board.cpp` vs `board/boardenginemove.cpp` (due path di move); `tt/zobrist.hpp` linee 69-116 (Zobrist).

## Output atteso
- Patta automatica non appena la posizione corrente ricorre per la terza volta, coerente con regola FIDE e con il contatore half-move già esistente.
- Stato hash e cronologia disponibili per funzioni di debug o GUI senza dipendere dalla Transposition Table.
