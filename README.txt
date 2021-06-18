// Author Josef E. Zerpa R.
// Codice Matricola 1837394

Istruzioni per l'uso:

- Compilare con 'make all'

- Aprire nuove finestre, una per il server, e una per ogni client (es 3 client).

- Creare una nuova cartella per ogni client per compartimentizzare i log files, ed evitare sovrascritture sullo stesso file di client diversi. 'mkdir client1'

- Copiare l'eseguibile 'client' in ogni cartella client creata. 'cp client client1/'

- Avviare il server con 'sudo ./server <portno>'

- Avviare i client con 'sudo ./client 127.0.0.1 <portno>'

- Inviare messaggi e uscire con CTRL-C


Funzionalità implementate:
- Connessione di N utenti alla chatroom.
- Condivisione di messaggi tra tutti gli utenti dela chatroom.
- Distibuzione ordinata dei messaggi, secondo l'ordine di invio e di arrivo al server.
- Creazione di log files globali e personali, rispoettivamente di tutti i messaggi e dei messaggi inviati.
- Notifica da parte del server di nuove connessioni e disconnessioni.
- Metadata nei messaggi, con l'id del mittente del messaggio e l'ora di invio. (TODO: ogni giorno a mezzanotte il server invierebbe una notifica della data, per poter vedere quando sono stati inviati i messaggi, e non solo a che ora. Stile Whatsapp.)
- 



Test effettuati:
- Demo di usabilità.
- Test di overflow con messaggi lunghi.
- 
