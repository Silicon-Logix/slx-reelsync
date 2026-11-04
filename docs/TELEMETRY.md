<!-- SPDX-FileCopyrightText: 2026 Marco Pezzullo (Silicon LogiX)
SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0 -->
# Contratto dati per la dashboard

Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).

La versione PC 1.2.0 esporta `telemetry.json` insieme ai risultati e fornisce
`Session::telemetry()` nel componente host. Il formato e indipendente dal trasporto.
Il servizio opzionale apre il canale TLS quando viene abilitato dalla pagina
Pannello Wi-Fi. Il core resta locale e non controlla hardware.

Schema: [telemetry-v1.schema.json](schemas/telemetry-v1.schema.json).

| Campo | Significato |
| --- | --- |
| schema | `slx.reelsync.telemetry.v1` |
| run_id | Identificativo opaco della prova, conservato al ripristino e sostituito con Nuova prova |
| sequence, sim_s | Passo numerico e tempo simulato; non un orologio di rete |
| state.code | Stato 0..7 secondo SDK; usare il codice per la logica e localizzare l'etichetta nel client |
| state.previous | Stato precedente, significativo durante Pausa analisi |
| connection | Origine dei dati, presenza hardware e disponibilita del controllo remoto |
| process | Grandezze del modello, con unita nel nome del campo |
| sensors | Misure simulate, incluse le alterazioni del sensore |
| alarms | Maschere attiva/memorizzata e causa iniettata |

La qualita sensori vale 0 per dato non valido, 1 nominale, 2 alterato.
Una lettura congelata di un encoder guasto non deve essere visualizzata come
una misura valida. I valori `process` permettono il confronto con i sensori.
Le potenze sono meccaniche, con segno; non sono consumi elettrici misurati.
Il quadrante pneumatico usa la forza firmata dell'attuatore in N, non una pressione in bar.
`nip_travel_m` integra la velocità periferica del nip: può avanzare anche dopo
una rottura e non misura la lunghezza a riposo effettivamente trasferita.

L'HMI nella scena legge lo stesso oggetto, con aggiornamento visuale a 10 Hz e
aggiornamento immediato nei cambi di stato. In pausa i valori restano fermi.
La modalita Presentazione non mostra misure numeriche di processo.

## Comandi

Schema: [command-v1.schema.json](schemas/command-v1.schema.json). Il decoder
`decodeCommand()` controlla formato, sessione destinataria, valori e identificativi.
Decodificare non esegue il comando. Il core applica ancora capability, stato,
sequenza, scadenza e limiti all'eventuale invio attraverso il trasporto autorizzato.

```json
{
  "schema": "slx.reelsync.command.v1",
  "id": "9007199254740993",
  "run_id": "rs-0123456789abcdef0123456789abcdef",
  "command": "set_speed",
  "value": 2.5,
  "expires_at_sim_s": 24.0
}
```

L'id e una stringa decimale per conservare la precisione dei 64 bit in JavaScript.
Deve essere monotono, non nullo, fino a UINT64_MAX-1. Il client deve usare il run_id
corrente: un comando di una prova precedente viene respinto. Le scadenze sono
in secondi simulati; 0 mantiene la convenzione locale senza scadenza del core.
Un futuro adattatore di rete dovra imporre scadenze esplicite e distinguere anche
la freschezza dei dati rispetto all'orologio del trasporto.

Comandi: start, stop, pause, resume, reset, set_speed, set_tension,
enter_maintenance, exit_maintenance, inject_fault, single_step, remove_fault,
acknowledge. `value` e obbligatorio solo per riferimenti e iniezione; `fault` solo
per iniezione. I campi inattesi vengono respinti. I campi e le maschere del core
sono documentati in SDK.md e FAULT_LAB.md.

## Eventi

`Session::eventFrame(after_sequence)` produce il contratto
`slx.reelsync.events.v1`, esportato anche in `events.json`.
[Schema eventi](schemas/events-v1.schema.json). Identificativi e cursori sono
stringhe decimali; il tempo resta numerico in secondi simulati. Con cursore 0
si leggono tutti gli eventi conservati della prova corrente. Un cursore troppo
vecchio o superiore all'ultimo evento imposta `resync_required`: il client deve
risincronizzare snapshot ed eventi, senza interpretare l'assenza di dati come
assenza di allarmi. Alla variazione di run_id si azzera il cursore.

Gli eventi di comando hanno tipo 1..14 e result come esito SDK. Gli eventi di
processo hanno tipo 100..105; nel tipo 105 result e la maschera allarmi, non
un codice di errore. Le sequenze sono ordinate e non devono essere confuse con
l'id del comando. Gli eventi respinti restano nella cronologia e nel ripristino.

## Collegamento Raspberry 1.2.0

La HMI nativa C++/LVGL comunica con un servizio locale tramite socket Unix privato.
Il servizio usa WebSocket su TLS 1.3 verso il PC (porta predefinita 47631), con
certificati di dispositivo e impronte SHA-256 della chiave pubblica. La prima
associazione richiede confronto dell'impronta del PC, codice valido due minuti
e conferma sul PC del pannello richiedente. Le identità si generano al primo
avvio; nessuna chiave o credenziale è preinstallata nell'immagine.

Il client è inizialmente in sola lettura. L'operatore richiede il controllo e il
PC lo assegna a una sola connessione. Le richieste includono lease, token recente
del trasporto e run_id; questi ultimi non sostituiscono l'autenticazione TLS.
Lo stesso run_id protegge anche ricette, piani, riferimenti ed esportazioni.
Il core conferma ancora capability, stato, identificativo e scadenza del comando.

La coda del trasporto scade in 500 ms. I token recenti sono limitati a otto
pubblicazioni del servizio locale; il pannello sospende i comandi con dati più
vecchi di un secondo. La freschezza usa orologi monotoni, separati dal tempo
simulato che può restare fermo in pausa. Un comando senza esito entro due secondi
chiude la sessione e richiede di verificare lo stato: non viene reinviato.

Gli esiti correlati riportano anche il prossimo identificativo e lo stato osservato
del core. Alla riconnessione si azzera la coda, si risincronizza lo snapshot e il
pannello torna in sola lettura. Il riavvio della prova richiede un comando esplicito.
Un comando locale sul PC revoca l'autorità remota.

`connection` nello snapshot SDK descrive la simulazione locale. La busta
`slx.reelsync.link.v1` aggiunge collegamento, età, destinatario e autorità per la
dashboard. L'IPC locale del PC richiede un token per processo; quello Linux usa
un socket con permessi 0600. I thread di trasporto non accedono a Session.
Step e invio dei comandi rimangono serializzati dal proprietario esistente.

Le operazioni host comprendono riferimenti, ricette predefinite, piani guasto,
arresto a metratura, pausa al primo allarme, esportazione e analisi in background.
Non accettano percorsi filesystem dal client. La rete locale funziona senza Internet.
Wi-Fi autenticato e PC disponibile sono due condizioni distinte.

## Geometria live dalla versione PC 1.2.0

`state.model_halted` indica l'arresto della prova al limite di validità;
allarme 256 ed evento 106 richiedono Nuova prova. Tempo e sequenza conservano
l'ultimo passo valido. Il campo è opzionale per i produttori v1 precedenti.

`visual` è un'aggiunta opzionale al contratto v1, schema
`slx.reelsync.visual.v1`. Contiene nove `rollers` nell'ordine del percorso,
ciascuno con `id`, `center_m=[X,Z]` nativo, `radius_m`, `phase_rad` intorno a +Y.
`path_m` contiene punti `[X,Z,distanza_geometrica]` del getter SDK, semplificati
con errore massimo 0.8 mm; distanza e estremi sono conservati. La quota Z
nativa coincide con la seconda coordinata Y-up esposta da GetPath.
`material` contiene origine e lunghezze a riposo e geometriche delle due zone,
oltre a `connected`. Le lunghezze visuali includono il distacco radiale 0.4 mm
previsto dal contratto della scena, separato dalle lunghezze fisiche del modello.

La dashboard 1.4 interpola due frame per 100 ms e non estrapola. I riferimenti
sulla lastra usano la coordinata materiale, non il tempo della Raspberry.
Il rullo trainante usa il proprio raggio 0.14 m; le fasi passive sono riferimenti
cinematici ideali, senza dinamica dei cuscinetti. Nessun secondo modello fisico
viene eseguito sulla scheda. Con server precedente lo schema resta in attesa.

## Dinamica del profilo virtuale PC 1.3.0

La forza pneumatica e le potenze sono valori effettivi degli stati dinamici.
`pneumatic_available_fraction` rappresenta la quota disponibile richiesta dal
guasto: la forza non salta immediatamente al nuovo valore. Motori e attuatore
continuano a evolvere soltanto con il tempo simulato. La nuova ricetta PET ha
id 2, con gli stessi parametri sul PC e sul pannello. Contratto visuale v1 e
layout ABI 1 conservati. Dettagli: [VIRTUAL_PLANT.md](VIRTUAL_PLANT.md).
