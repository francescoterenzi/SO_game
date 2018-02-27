## PROPOSTA PER IL SERVER

Il server rimane in ascolto su TCP, quando un nuovo client
si connette il server crea un veicolo con l'id associato al client
e l'immagine di default.
Successivamente il server entra in un ciclo.
- Se il client si disconnette il server esce dal ciclo e rimuove il client dal mondo
- Se il client manda una PostTexture allora il server cambia la vehicle texture di default
    con quella che ha mandato il client
- Se il client manda una GetTexture il server cerca nel mondo l'immagine corrispondente all'id
    richiesto dal client (infatti ci possono essere due casi, nel primo caso il client vuole usare
    l'immagine di default e manda il suo id, nel secondo caso vuole conoscere la texture di un secondo
    veicolo che è entrato nel gioco, e quindi manda l'id di quel veicolo).
- Il resto come prima

Il vantaggio di fare così è che ti eviti due cicli.