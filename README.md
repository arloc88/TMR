# TMR
Meccanismo di triple modular redundancy (TMR) per la tolleranza ai guasti

Il progetto fa riferimento ad un sistema con patch RTAI con versioni:
    - 3.8.13 del kernel Linux
    - 4.0 di RTAI


/*------------------- CONTROLLER.C ---------------------*/

Al controller originale sono state aggiunte due repliche del task control con algorithm diversity (è stato cambiato l'ordine delle istruzioni).

Ogni control_Task calcola il proprio errore, elabora la relativa legge di controllo la invia tramite mailbox al task voter 
e la salva in una memoria condivisa con il task aperiodico gather "gatMem".

Il task VOTER riceve tramite mailbox le leggi di controllo e decide a maggioranza la control action definitiva (anche questa viene salvata
nella memoria condivisa gatMem).

Il task SERVER intercetta una richiesta aperiodica effettuata dal task aperiodico "diag" tramite una memoria condivisa "shared" e per
servirla invoca una funzione con cui viene inviato
 al task gather un messaggio tramite mailbox per segnalare una richiesta aperiodica e
recuperare lo stato del sistema. 



/*------------------- GATHER.C ---------------------*/

Il task aperiodico gather è in attesa di una richiesta aperiodica, quando questa arriva il gather viene invocato dal Server DS tramite 
mailbox quindi raccoglie le informazioni sullo stato 
del sistema dalla memoria condivisa gatMem e le invia trmaite mailbox al task diag



/*------------------- DIAG.C ---------------------*/

Il task aperiodico diag permette di effettuare una richiesta aperiodica premendo il tasto 1 
che lasegnala al Server, il server invocherà il task gather tramite mailbox.

La richiesta viene servita dal gather che invia al diag le informazioni sul sistema.
Il diag riceve dal gather le info sul sistema tramite mailbox e le stampa a video.

*/
