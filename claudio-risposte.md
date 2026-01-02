*** Usando @agent-code-reviewer, verifica se sia presente codice non usato. 

● I'll use the code-reviewer agent to analyze the codebase for unused code.

● code-reviewer(Review codebase for unused code)
  ⎿  Done (46 tool uses · 109.0k tokens · 2m 9s)                        

● Ho completato l'analisi del codice per identificare il codice non utilizzato. Ecco un riepilogo dei principali risultati:

  Codice Non Utilizzato Trovato

  🔴 Rimozione Sicura - Alto Impatto

  2. Variabili membro in Board (board/board.hpp:395-396):
    - whiteKingIndex e blackKingIndex - dichiarate per caching ma mai utilizzate

  🟡 Codice Commentato - Da Rimuovere

  - printer/prints.cpp:59-143 - ~85 righe di vecchio codice commentato
  - engine/basicrules.hpp:14-55 - funzioni di valutazione commentate
  - main.cpp:18-34 - codice di test bot vs bot commentato

  📊 Impatto Totale

  Rimuovendo tutto il codice identificato:
  - ~500+ righe di codice in meno
  - 2 file completi eliminabili (modulo gamestatus)
  - Codice più pulito e manutenibile
  - Nessun impatto sulla funzionalità (codice dimostrabilmente non usato)

  Vuoi che proceda con la pulizia del codice? Posso iniziare rimuovendo le parti più sicure (modulo gamestatus, variabili non usate, codice commentato) oppure preferisci rivedere ogni elemento prima della rimozione?

*** Utilizzo l'agente refactoring-expert per identificare le funzioni più grandi che necessitano refactoring.

● refactoring-expert(Identify large functions needing refactoring)
  ⎿  Done (11 tool uses · 73.0k tokens · 1m 57s)                                                               

● Ecco l'analisi completa delle funzioni più grandi che necessitano refactoring, ordinate per priorità:

  🔴 PRIORITÀ CRITICA

  1. Board::canMoveToBB() - 273 righe

  Posizione: board/board.cpp:191-463

  Problemi:
  - Funzione enorme con responsabilità multiple
  - 8+ livelli di nesting, 100+ branch condizionali
  - Gestisce tutti i 6 tipi di pezzi con logiche diverse
  - Logica duplicata per la sicurezza del re (ripetuta più volte)
  - Praticamente impossibile da mantenere/debuggare

  Strategia: Estrarre metodi separati per tipo di pezzo (canPawnMoveTo(), canKnightMoveTo(), ecc.), separare validazione da controlli sicurezza re

  ---
  2. Board::doMove() - 184 righe

  Posizione: board/board.cpp:700-883

  Problemi:
  - Troppe responsabilità: esecuzione mossa, en passant, arrocco, promozione, diritti arrocco
  - 50+ righe solo per logica diritti arrocco
  - Calcoli ripetuti, casi speciali sparsi ovunque

  Strategia: Estrarre handleEnPassantCapture(), handleCastling(), updateCastlingRights(), handlePromotion()

  ---
  3. Board::hasAnyLegalMove() - 115 righe

  Posizione: board/board.cpp:576-690

  Problemi:
  - Stesso pattern ripetuto 6 volte (uno per ogni tipo di pezzo)
  - Codice duplicato: loop quasi identici

  Strategia: Estrarre hasLegalMovesForPieceType(), usare dispatch table con function pointers

  ---
  🟡 ALTA PRIORITÀ

  4. Engine::evaluate() - 72 righe

  Posizione: engine/evaluate.cpp:628-699

  - Orchestra 20+ funzioni di valutazione
  - Passaggio parametri ripetitivo
  - Difficile capire contributo di ogni componente

  Strategia: Introdurre EvaluationContext, usare Strategy Pattern per fase di gioco

  ---
  5. Engine::staticExchangeEvaluation() - 115 righe

  Posizione: engine/search.cpp:573-687

  - Due grandi lambda nested
  - Difficile da testare in unit test

  Strategia: Estrarre lambda come metodi privati

  ---
  6. Driver::playerTurn() - 127 righe

  Posizione: driver/driver.cpp:210-336

  - Loop validazione input massiccio con 10+ controlli
  - 4-5 livelli di nesting
  - UI mescolata con logica di gioco

  Strategia: Separare parsing, validazione, esecuzione; separare UI da logica

  ---
  📊 Tabella Riassuntiva

  | Funzione                 | Righe | Priorità | Effort     | Rischio |
  |--------------------------|-------|----------|------------|---------|
  | canMoveToBB              | 273   | CRITICA  | 3-5 giorni | Alto    |
  | doMove                   | 184   | CRITICA  | 2-3 giorni | Alto    |
  | hasAnyLegalMove          | 115   | ALTA     | 1 giorno   | Medio   |
  | evaluate                 | 72    | ALTA     | 2 giorni   | Basso   |
  | staticExchangeEvaluation | 115   | ALTA     | 2 giorni   | Medio   |
  | playerTurn               | 127   | ALTA     | 1 giorno   | Basso   |

  ---
  🎯 Ordine Raccomandato

  1. Inizia con basso rischio: sortLegalMoves(), playerTurn(), hasAnyLegalMove() - pattern chiari, rischio basso
  2. Sistema di valutazione: Refactor evaluate() - migliora testabilità senza rompere engine core
  3. Move generation (ultimo): canMoveToBB(), doMove(), moveBB() - sono nel percorso critico, alto rischio ma massimo beneficio

  ---
  Vuoi che inizi il refactoring di qualche funzione specifica? Consiglio di partire da hasAnyLegalMove() o sortLegalMoves() per fare pratica prima di affrontare le critiche canMoveToBB() e doMove().


*** Con @agent-refactoring-expert potresti informarmi di quelle che sono le funzioni poco chiare che necessitano 
un refactor? 

● Utilizzo l'agente refactoring-expert per identificare le funzioni poco chiare che necessitano refactoring.

● refactoring-expert(Identify unclear functions needing refactoring)
  ⎿  Done (14 tool uses · 90.9k tokens · 2m 20s)                                                               

● Ecco l'analisi completa delle funzioni poco chiare che necessitano refactoring per migliorare la leggibilità:

  🔴 PROBLEMI CRITICI DI CHIAREZZA

  1. Board::canMoveToBB() - 272 righe

  Posizione: board/board.cpp:191-463

  Problemi di chiarezza:
  - Nome non descrive la complessità: sembra un semplice check ma simula la mossa con bitboard
  - Logica di "double check" non spiegata: if (attackerCount >= 2 && fromType != KING) - perché proprio 2?
  - Numeri magici per indici (27, 35, 18, 21) senza spiegazione
  - Conversione coordinata 7 - rank senza commento
  - Side effects nascosti con isKingAttackedCustom

  Raccomandazioni:
  - Rinominare in isMoveLegal() o validateMoveLegality()
  - Aggiungere commenti dettagliati sulla simulazione bitboard
  - Estrarre isPositionSafeAfterMove() come metodo separato

  ---
  2. Board::doMove() / Board::undoMove()

  Posizione: board/board.cpp:700-947

  Problemi di chiarezza:
  - Nome fuorviante: "doMove" suona temporaneo ma modifica permanentemente lo stato
  - Operazioni bit criptiche per diritti arrocco:
  const uint8_t kingBit = (movingColor == WHITE) ? 0x01 : 0x08;  // ???
  const uint8_t castleMask = (movingColor == WHITE) ? 0x03 : 0x0C;  // ???
  - Struct MoveState con 15+ campi: non chiaro quali sono obbligatori
  - Dipendenze nascoste: assume bitboard sincronizzati con array

  Raccomandazioni:
  - Rinominare in makeMove()/unmakeMove() (terminologia standard chess programming)
  - Documentare campi MoveState e ordine operazioni
  - Estrarre updateCastlingRights(move, state) con commenti chiari

  ---
  3. Engine::searchPosition()

  Posizione: engine/search.cpp:318-380

  Problemi di chiarezza:
  - Nome generico non indica algoritmo (negamax + alpha-beta)
  - Codice commentato di null-move pruning: feature futura o codice morto?
  - MAX_PLY - 1 check senza spiegazione del perché
  - Valore ritorno ambiguo: score da che prospettiva? Convenzione segno non chiara
  - SearchContext ridondante: depth, alpha, beta passati sia in struct che separati

  Raccomandazioni:
  - Rinominare in negamaxAlphaBeta()
  - Aggiungere header comment:
  // Negamax search con alpha-beta pruning
  // Ritorna score dalla prospettiva di chi muove (positivo = vincente)
  // @param depth: profondità rimanente (0 = nodo foglia)
  // @param alpha: lower bound (minimo garantito)
  // @param beta: upper bound (massimo garantito)

  ---
  4. Engine::generateLegalMoves()

  Posizione: engine/search.cpp:410-494

  Problemi di chiarezza:
  - Commento fuorviante: dice "pseudo-legal" ma valida con canMoveToBB
  - Arrocco con aritmetica indici oscura:
  // Cosa significano +2 e -2? Perché proprio questi offset?
  if (f <= 5 && b.canMoveToBB(fromC, Coords{uint8_t(from + 2)}, inCheck))
  - Validazione inconsistente tra re e altri pezzi

  Raccomandazioni:
  - Correggere commento ingannevole
  - Estrarre tryAddCastlingMoves() con costanti nominate:
  constexpr uint8_t KINGSIDE_OFFSET = 2;
  constexpr uint8_t QUEENSIDE_OFFSET = -2;

  ---
  5. Board::get() (overloads multipli)

  Posizione: board/board.hpp:103-125

  Problemi di chiarezza:
  - Conversione 7 - rank CRITICA ma non spiegata
  - Due sistemi coordinata mescolati senza documentazione:
    - Coords convention: a8=0, h1=63
    - Internal storage: chessboard[0]=row 1, chessboard[7]=row 8
  - (file << 2) è moltiplicazione per 4, non ovvio
  - get(row, col) vs get(index): quale usa quale convenzione?

  Raccomandazioni:
  - Aggiungere commento esaustivo:
  // CRITICO: Due sistemi coordinata coesistono:
  // 1. Coords: a8=index 0, h1=index 63 (rank 0 = riga 8)
  // 2. Storage interno: chessboard[0] = riga 1, chessboard[7] = riga 8
  // Formula conversione: storageRow = 7 - coordsRank
  - Rinominare get(row, col) in getInternal(row, col)

  ---
  🟡 PROBLEMI MODERATI

  6. Engine::staticExchangeEvaluation() - 115 righe

  Posizione: engine/search.cpp:573-687

  Problemi:
  - Acronimo "SEE" non universalmente noto
  - Lambda nested di 50+ righe ciascuna
  - Ottimizzazione early exit non spiegata:
  // Perché questa ottimizzazione funziona?
  if (side == sidePassive && gain[depth] > 0) {
      while (--depth > 0) {
          gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
      }
  }

  Raccomandazioni:
  - Aggiungere commento: "SEE = Static Exchange Evaluation"
  - Estrarre lambda come metodi privati
  - Spiegare ottimizzazione (negamax pruning in SEE)

  ---
  7. Engine::addMVVLVABonus()

  Posizione: engine/search.cpp:496-513

  Problemi:
  - Acronimo criptico "MVV-LVA" non spiegato
  - Rilevamento en passant silenzioso (mossa diagonale su casella vuota)

  Raccomandazioni:
  - Aggiungere commento:
  // MVV-LVA (Most Valuable Victim - Least Valuable Aggressor):
  // Prioritizza catture che prendono pezzi preziosi con pezzi economici

  ---
  8. poplsb()

  Posizione: engine/search.cpp:383-388

  Problemi:
  - Nome oscuro (jargon bitboard)
  - Side effect nascosto: modifica il parametro bitboard

  Raccomandazioni:
  - Rinominare in popLeastSignificantBit()
  - Aggiungere: // Estrae e azzera il bit meno significativo, ritorna il suo indice

  ---
  ⚠ INCONSISTENZE NOMENCLATURA

  9. Confusione Index/Square

  Funzioni diverse usano: index, sq, squareIndex, targetIndex, fromIndex, kingSq

  Raccomandazione: Standardizzare su un solo termine (es. square o index)

  ---
  10. Confusione Color/Side

  - A volte side (0/1)
  - A volte color (WHITE=0x0/BLACK=0x8)
  - A volte isWhite (bool)

  Raccomandazione: Aggiungere helper di conversione documentati:
  inline uint8_t colorToSide(uint8_t color) { return (color == WHITE) ? 0 : 1; }

  ---
  11. Suffisso "BB" Inconsistente

  Alcune funzioni bitboard hanno suffisso BB (moveBB, canMoveToBB), altre no (generateLegalMoves)

  Raccomandazione: Usare BB consistentemente o rimuoverlo completamente

  ---
  🔢 NUMERI MAGICI

  Numeri hardcoded sparsi senza costanti nominate:
  - 27, 35, 18, 21 in evaluateBlockedCenterWithPiecesFast
  - 60, 62, 4, 6 per posizioni arrocco
  - 56, 63, 0, 7 per torri iniziali
  - MAX_PLY = 64 - perché proprio 64?

  Raccomandazione: Definire costanti con commenti:
  constexpr uint8_t E4_INDEX = 28;
  constexpr uint8_t D4_INDEX = 27;
  constexpr uint8_t WHITE_KING_START = 60;  // e1

  ---
  📋 RIEPILOGO PRIORITÀ

  Top 5 funzioni da refactorare per chiarezza:

  1. Board::get() - Sistema coordinata non documentato causa bug potenziali
  2. Board::canMoveToBB() - Logica complessa senza spiegazioni
  3. Engine::searchPosition() - Algoritmo core poco chiaro
  4. Board::doMove()/undoMove() - Operazioni bit criptiche
  5. Engine::generateLegalMoves() - Commenti fuorvianti

  Pattern comuni di confusione:
  - Operazioni bitwise senza spiegazione
  - Indici hardcoded senza costanti nominate
  - Sistemi coordinata mescolati (Coords vs storage interno)
  - Nomi abbreviati da jargon chess programming
  - Commenti italiani mescolati con inglese
  - Codice commentato senza spiegazione
  - Convenzioni inconsistenti (BB suffix, side vs color)

  Vuoi che inizi il refactoring di qualche funzione specifica per migliorarne la chiarezza? Consiglio di partire da Board::get() (aggiungere documentazione sistema coordinata) e Engine::addMVVLVABonus() (semplice da documentare) come primi passi a basso rischio.
