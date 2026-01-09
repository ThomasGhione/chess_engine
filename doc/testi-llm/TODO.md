# TODO list

Ciao, non sai cosa fare? Ecco una serie di cose su cui ti puoi buttare.
Previligi sistemare cose rispetto a fare di nuove da zero.

Preso in carico da []
Stato: nessuno.

1. C'e' da effettuare un refactor di Board.
In questo momento ci sono tanti metodi in board.hpp ed e' di difficile lettura.
Inizialmente questa cosa era per avere maggiore efficienza.
Quando questa classe e' stata creata avevamo solo metodi inline.
Ora che la situazione non e' piu' cosi' possiamo permetterci senza problemi di
creare piu' file.

1.1. Spostare metodi in piu' file.
1.2. Ridurre la dimensione dei vari metodi.
Ci sono dei metodi dentro Board che sono di tante righe.
Cerca di ridurre le indentazioni e aumenta il numero di funzioni ausiliarie.

Preso in carico da []
Stato: nessuno.

2. Va modificato il main per adattarlo, prima avevamo Driver che gestiva tutto.
Con il cambio di struttura del codice ora abbiamo come tramite 'stato0'.

2.1 Elimina il codice vecchio del main e inserisci una chiamata a stato0.

Preso in carico da []
Stato: nessuno.

3. C'e' da effettuare un refactor di metodi bit-board.
Essendo stati scritti da AI, ci sono degli evidenti problemi di lettura.

3.1 Elimina le indentazioni


Preso in carico da []
Stato: nessuno.

4.  C'e' da effettuare un refactor di Engine.
Le ricerca va riscritta, in questo momento e' molto criptica e lunga in termini
di righe di codice per funzione.

4.1 Riduci il numero di righe per funzione.

Preso in carico da []
Stato: nessuno.

5. Scrivere i test!
Per tipo quasi tutto ¯\_(⊙︿⊙)_/¯
