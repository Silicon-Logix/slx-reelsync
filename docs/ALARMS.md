<!-- SPDX-FileCopyrightText: 2026 Marco Pezzullo (Silicon LogiX)
SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0 -->
# Allarmi e prove

Il processo è interamente simulato sul PC. La Raspberry è un pannello
operatore collegato al processo e non esegue un secondo modello fisico.

| Codice | Condizione rilevata | Zona evidenziata |
|---|---|---|
| E001 | Encoder non valido | Traino / encoder |
| E002 | Discordanza velocità | Traino / encoder |
| E003 | Scostamento tensione | Cella di carico |
| E004 | Finecorsa ballerino | Ballerino e servizio pneumatico |
| E005 | Coppia satura persistente | Azionamenti |
| E006 | Limite operativo | Linea |
| E007 | Materiale rotto | Percorso lastra / nip |
| E008 | Controllore non valido | Linea / controllo |
| E009 | Validità del modello superata | Linea |

Rosso indica allarmi attivi. Ambra indica memoria di un allarme rientrato o
una causa di prova, identificata esplicitamente come tale. I simboli e i
testi accompagnano sempre il colore. Le istruzioni indicano cosa verificare;
non costituiscono una diagnosi automatica certa di un impianto fisico.

Un errore scala encoder o un bias possono essere presenti prima che una
soglia diagnostica sia superata. La causa impostata nel laboratorio e la
diagnosi del controllo sono informazioni distinte. Una conferma non elimina
una causa attiva né aggira la frenatura o i limiti del modello.

Per il video, lasciare la Raspberry in profilo Operatore. Preparare
l'anomalia nel laboratorio PC, impostare un ritardo e premere **Avvia conto
alla rovescia**. Il simulatore apre il 3D e il pannello mostra il tempo
rimanente. Dopo l'attivazione, mostrare il componente e aprire Allarmi sulla
scheda. Rimuovere la causa dal PC, attendere il rientro e l'arresto richiesto,
poi confermare il ripristino dalla dashboard. La rottura richiede una nuova
prova; il reset non è un riconoscimento di allarme.

I timer usano secondi del modello: a ritmo 1x corrispondono al tempo della
prova, a ritmo maggiore si accorciano rispetto al tempo reale. La pausa li
congela. **Annulla timer** elimina una prova ancora in attesa. Dopo una perdita
del collegamento i dati sono indicati come non aggiornati e i comandi sono
sospesi. Il profilo Laboratorio è una separazione dell'interfaccia per le
prove, non un sistema di autorizzazione industriale a ruoli.
