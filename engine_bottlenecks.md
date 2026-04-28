# Engine Bottlenecks

## Inquadramento

Il front-end `Engine` e' leggero: `searchUCI()`/`search()` fanno coordinamento, stop del ponder, setup dello stato e delegano quasi subito a `Searcher`.
I colli principali sono quindi nel percorso `Engine -> Searcher -> MoveGenerator/Sorter/Evaluator`.
Le priorita' sotto sono ordinate per impatto atteso su nodi/secondo, branching e stabilita' dei tempi.

## Priorita'

| Priorita | Bottleneck | Dove | Causa | Fix consigliato |
|---|---|---|---|---|
[V]| P0 | SEE calcolata troppo presto | `engine/search/sorter.cpp` | `sortLegalMoves()` calcola `staticExchangeEvaluation()` per ogni cattura prima di sapere se la mossa sara' davvero cercata. SEE richiama attacchi sliding in un loop di scambi. | Rendere SEE lazy: hash move senza SEE, catture materialmente ovvie con score rapido, SEE solo per catture sospette. |
[V]| P0 | QSearch costosa | `engine/search/searcher.cpp`, `engine/search/sorter.cpp` | Ogni nodo qsearch valuta, genera tattiche e filtra molte catture con SEE. | Staged tactical move picker: delta pruning economico prima, SEE solo sui candidati rimasti. |
[ ]| P1 | Evaluator / king safety | `engine/eval/general/evaluate.cpp`, `engine/eval/king/*` | `computeAttackData()` costruisce attacchi completi e king safety e' il benchmark helper piu' pesante. | Calcolare attack data solo quando serve, valutare cache piu' associativa/grande. |
[ ]| P1 | Legal move generation completa | `engine/search/move_generator.cpp` | Ogni nodo genera liste complete con pin rays, king legality e array locali. | Generatori specializzati: evasion-only, tactical-only, staged legal generation. |
[ ]| P1 | Micro-overhead nel loop caldo | `engine/search/searcher.cpp` | Per ogni mossa si ripetono controlli su board, promotion, EP, fork threat e material delta non sempre cached. | Reusare metadati gia' calcolati e usare funzioni cached/incrementali. |
[ ]| P1/P2 | Root parallel / YBWC | `engine/search/searcher.cpp` | Board copy per task, alpha condivisa solo in merge, worker senza TT write/heuristic updates. | Benchmark `MAX_THREADS=1/2/N`; abilitare YBWC solo dove vince o introdurre shared alpha atomico. |
[ ]| P2 | Stop ponder blocking | `engine/engine.cpp` | `searchUCI()` chiama sempre `stopPondering()` e puo' attendere `join()`. | Misurare latenza con ponder attivo e riusare il risultato ponder quando possibile. |
[ ]| P2 | Cache evaluator direct-mapped | `engine/eval/general/evaluate.cpp`, `engine/eval/general/attack_data.cpp` | Cache piccole e direct-mapped, quindi collisioni probabili in search larga. | Provare size tuning o cache 2-way/4-way con benchmark A/B. |

## Guardrail Di Misura

- Baseline principale: `make perf && ./tests/perf`.
- Metriche minime: tempo depth 11, nodi visitati, media depth 10, `generateLegalMoves` benchmark.
- Regola pratica: un cambio performance e' buono solo se non aumenta sensibilmente i nodi a parita' di profondita'.
