# Stato attuale

Quando dobbiamo fare tuning andiamo un po' a sentimento.
O meglio, sfruttiamo una serie di script per poter fare questa cosa.
Uno script data una modifica dice se il modello e' piu' forte di quello di prima o meno.
L'altro script invece, tenta con la discesa del gradiante a trovare gli ottimi.

Il problma di questi script e' che sono lenti in primis.
Inoltre, questi script hanno la miopia di poter vedere solo parte del sistema.

In questo momento possiamo immaginare il nostro sistema come una sorta di "black-box".

f : R^n -> R

Dove n rappresenta la cardinalita' delle costanti che siamo riusci ad estrarre dal processo
di valutazione.

# Proposta

Usiamo un sistema statistico detto CMA-ES.
L'idea alla base sfrutta un principio molto simile alla discesa del gradiante.

Il problema della funzione 'f' descritta e' che non possiamo fare la derivata parziale.
In secondo luogo, non e' molto efficiente avendo un n grande.
Inoltre, di fatto, non facciamo mai la ricerca del gradiante su 'f'.

# Su cosa facciamo la ricerca del gradiante?

Data una posizione di partenza 'pos'.

g : (pos,f) -> risultato_partita

Dove risultato_partita: {+1, +1/2, +0}

Noi vogliamo massimizzare:

max sommatoria di g(pos, f)

# Idea per velocizzare il processo

Usare un motore NNU deterministico.
L'ida e' quella di cercare di velocizzare il processo "costoso"
di calcolo di g.

Evitiamo poi di attivare il "pensiero nel tempo dell'avversario" e "gestione del tempo".
Fissiamo una profondita' e da li dovremmo avere una velocita' abbastanza sostenuta anche da
parte del nostro motore.

# Come mai questo sistema e' piu' efficacie

- CMA-ES e' ideato proprio per i sistemi black box.
- Abbassiamo di molto il costo di g.
- Con l'algoritmo probabilistico ci avviciniamo piu' velocemente della discesa del gradiante

# Infrastruttura necessaria

1. Integrazione di tutti i parametri con CMA-ES

Processo di CMA:

2. CMA-ES genera dei candidati di esplorazione
3. Ogni candidato va testato
4. CMA-ES aggiorna la sua matrice interna

(Processo ripetuto)

# Come testiamo i nuovi motori?

Va creato uno script che preso in ingresso i parametri del bot:

1. Compili il progetto con i realtivi parametri
2. Faccia fare Bot vs Motore NNU (per numero di round)
3. Collezione il risultato di funzione g.


> https://github.com/CMA-ES/c-cmaes

# Difficolta' 

Riuscire a crare integrazione con progetto CMA-ES, il nostro motore e un motore esterno.
