-- Situazione attuale
Il bot non usa in modo "furbo" il tempo.
Che sia una partita a tempo veloce o lento,
tende a giocare sempre molto velocemente.

-- Questo va bene oppure e' un problema?
Spieghero' di seguito in quali situazioni
questo comportamento risulta comodo.
In generale pero' faccio una riflessione
piu' ampia. Questa cosa NON va sempre bene.
In una partita a 30' usare pochi secondi per
mossa risulta una gestione del tempo decisamente
cattiva.

-- Il tempo e' una risorsa imporante
Ci possiamo concentrare molto sull'avere
il massimo dell'accuratezza.
Tuttavia, perdere per il tempo e' sempre
una possibilita' dietro l'angolo.
Nota: per entrambi i giocatori e' una possibilita'!


-- Requisito utente
Vorremmo che in delle partite a tempo,
il bot sfruttasse meglio il tempo a sua disposizione.
Prendendosi piu' o meno tempo in base ad una serie
di criteri.

-- Cosiderazioni di natura generle

++ Vorremmo idealmente che la profondita' fosse infinita.
Tuttavia, non abbiamo tempo infinito.
Quindi, la mia proposta e' di stabilire una soglia detta X.
Questa e' una soglia sotto cui, idealmente, non vorremmo mai
dover scendere.

Proposta di X:
X -> Numero di mosse per poter dare matto con K+N+B (situazione peggiore)

Proposta di K:
K -> Numero di ricerca massimo, non posso aumentare piu' di questo numero.
Se aumentassi ancora i tempi di ricerca sarebbero troppo elevati.

Con questa proposta, essendo nella situazione "peggiore",
siamo sicuri di avere una ricerca di qualita'.
Se finissimo sotto X potremmo avere dei risultati della ricerca
degradati.


++ A inizio partita devo capire la modalita' di gioco.
Se giochiamo a tempo:
ALLORA vorrei sapere il t_max e l'eventuale t_incremento.

++ SE abbiamo t_incremento:
Posso "regalamrmi" un tempo minimo per fare la mossa.

++ Se stiamo giocando a tempo:
ALLORA salvo in delle variabili t_m e t_a.
Rispettivamente il mio tempo e quello dell'avversario.

Durante la partita devo fare queesti controlli con i due dati.

- Se t_a < SOGLIA allora gioco per fare flag.
- Se t_m < SOGLIA allora gioco sotto X, non voglio perdere per tempo.
- Se t_m - t_a > 0 allora posso prendere piu' tempo per mossa
- Se t_m - t_a < 0 
  SE scarto grosso allora gioco sotto X, non volgio perdere per tempo.
  SE scarto piccolo, allora posso anche ignorare il problema.

NOTA: lo scarto va calcolato in percentuale al MAX(t_a, t_m) = 100%.
NOTA 2: Alcuni bot tendono a giocare molto a "speedrun" di fatto
usando solo l'incremento.
Se MAX(t_a, t_partita) = t_partita allora ignoro uso \Delta t.
(\Delta t definitio dopo)


++ Per una corretta gestione del tempo devo cosiderare l'eval
corrente.

- Se eval a sfavore: mi serve aumentare la precisione aumentando la ricerca.
- Se eval a favore: potrei decidere di diminuire la profondita' di ricerca
(NON sotto soglia X, e NON sopra K)

++ Il tempo medio per mossa dovrebbe essere:

\Delta t circa (t_max) / n_mosse

dove n_mosse rappresenta una media di mosse per arrivare
a fine partita.

!! Questo pero' non vale ad inizio partita.

Mi aspetto che il tempo medio per le mosse all'inizio
sia molto infriore a \Delta t.


-- Organizzazione finale di un algoritmo alto livello:
Idealmente questi controlli dovrebbero essere in parallelo.

Definizione delle variabili:
\Delta t circa (t_max) / n_mosse
t_a
t_m
t_min = incremento

CRITERI PER GIOCARE SOTTO SOGLIA: (Sono in OR)
- t_m < SOGLIA
- t_m - t_a < \Delta Accettabile

CRITERI PER DIMINUIRE FINO A MASSIMO SOGLIA X: (Sono in OR)
- Mosse apertura
- Se Eval MOLTO a favore
- Se t_m < t_a
- Se t_a < SOGLIA

CRITERI PER AUMENTARE FINO A MASSIMO SOGLIA K: (Sono in OR)
- Se mediamente sto gicando sempre sotto il tempo medio per mossa
- Se eval sfavorevole
- t_m - t_a > SOGLIA accettabile, posso rallentare, ho tanto tempo in piu'.

Se NON soddisfiamo i requisiti sopra citati:
LASCIAMO INVARIATO.
