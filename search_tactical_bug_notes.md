# Search Tactical Bug Notes

## 1. QSearch SEE pruning troppo aggressivo

- File: `engine/search/sorter.cpp`
- Area: `Sorter::sortTacticalMoves()`
- Rischio: la qsearch scarta catture con `SEE < -15/-8/0` e poi applica delta pruning usando `see + 100`.
- Sintomo atteso: il motore puo' sovrastimare sacrifici o mosse di scacco se la confutazione richiede una cattura temporaneamente negativa, un desperado o una sequenza forzante.
- Fix scelto: tuning locale a `SEE < -24/-12/-4` e `see + 140`.
- Benchmark A/B su `./tests/perf`: baseline rebuild pulita `19,648,153` nodi depth 11; tuning scelto `12,592,624` nodi depth 11 e test passati. Una variante piu' larga `-32/-16/-4` con `see + 180` ha fallito l'avg depth 10.

## 2. QSearch senza quiet checks

- File: `engine/search/move_generator.cpp`
- Area: `generateTacticalMoves()` / `generateQSearchTacticalMoves()`
- Rischio: fuori scacco la qsearch genera solo catture e promozioni, non check quiet.
- Sintomo atteso: linee tattiche con sacrificio + scacco possono essere troncate prima di vedere risorse quiet forzanti.

## 3. Main search pruning prima di sapere se una quiet da' scacco

- File: `engine/search/searcher.cpp`
- Area: `Searcher::searchMoves()`
- Rischio: LMP/futility possono scartare quiet non-pedone prima del `doMove()` e quindi prima di sapere se danno scacco.
- Sintomo atteso: mosse difensive/forzanti rare possono sparire quando sono ordinate tardi.

## 4. SEE fragile su en passant

- File: `engine/search/sorter.cpp`
- Area: `Sorter::staticExchangeEvaluation()`
- Rischio: la SEE interpreta destinazione vuota come en passant e, nel caso EP, non mette esplicitamente il pedone mosso sulla casa di arrivo nell'occupancy simulata.
- Sintomo atteso: raro mis-score di EP tattiche; non sembra il colpevole principale, ma e' un bug fragile da chiudere.

## Critical position 18

- FEN: `2r1r1k1/1p3pp1/2b1p2p/p2p2b1/Pq1P4/2NQ1N2/RPP2PPP/4R1K1 w - - 4 20`
- Mossa da vietare: `d3e3` (`Qe3`), perche' `Bg5xe3` cattura immediatamente la donna.
- Stato indagine: con engine fresco, sia con vecchi valori qsearch sia con tuning `-24/-12/-4` + `see + 140`, il motore non gioca `Qe3` a depth 1-10. Se `Qe3` viene forzata, il nero trova `g5e3` gia' a depth 1.
- Ipotesi residua: se la partita reale ha prodotto `Qe3`, il problema e' piu' probabilmente legato a stato di ricerca in partita (history/TT/ponder/result reuse), binario stale, oppure FEN/stato leggermente diverso, non a move generation base della cattura.
