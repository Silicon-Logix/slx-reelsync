# Laboratorio guasti — SLX ReelSync 0.2.0

Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0

Le anomalie cambiano parametri o segnali del core. I grafici distinguono processo,
misure e riferimenti; non vengono disegnate tracce preconfezionate.
È attiva una sola causa iniettata per volta. Allarmi conseguenti possono coesistere.

| Causa | Parametro | Effetto |
| --- | --- | --- |
| Perdita encoder | nessuno | Lettura congelata, qualità non valida, frenatura memorizzata |
| Bias cella | -20..20 N | Offset nel feedback di tensione |
| Attrito | fattore 1..10 | Attrito viscoso dei tre azionamenti |
| Rottura | nessuno | Tensioni e flussi nulli; frammenti conservati nel bilancio |
| Derating azionamenti | quota disponibile 0.1..1 | Limite effettivo di coppia ridotto, saturazioni e risposta delle rampe |
| Perdita forza pneumatica | quota residua 0..1 | Richiesta di forza ridotta, risposta progressiva; peso presente nel bilancio |
| Scala encoder | fattore 0.5..1.5 | Misura di velocità alterata e residuo osservabile |
| Cella bloccata | nessuno | Ultima misura conservata mentre la tensione reale può cambiare |
| Ritardo sensori | 0.005..0.2 s, multipli di 5 ms | Ritardo aggiuntivo su encoder e tensione tramite storia dei campioni |

Il precarico pneumatico è una forza equivalente; non viene simulata o misurata
una pressione. Il quadrante 3D mostra la forza effettiva in N. L'encoder della linea è distinto
dai feedback ideali interni al modello dei servomotori.

In **Guasti e allarmi**, scegliere causa e intensità. Si può iniettare subito
oppure programmare un istante in tempo simulato. Durata 0 significa persistente;
una durata positiva rimuove la causa all'istante iniziale più la durata.
Massimo 32 azioni pianificate. Le azioni future non avanzano durante una pausa.
Iniezioni incompatibili con lo stato o con una causa già attiva vengono respinte
dal core e registrate; la tabella mostra l'esito reale.

**Rimuovi causa** conserva bobine, deformazioni, integrali, tempo e registrazione.
Una frenatura di guasto resta memorizzata: attendere condizioni stabili e assenza
di allarmi attivi, quindi usare **Conferma ripristino**. La rottura e un output
non valido del controllore richiedono una **Nuova prova**. Il ripristino non
costituisce una funzione di sicurezza di una macchina reale.

Le **Prove di errore operatore** inviano intenzionalmente un riferimento di
5 m/s, fuori campo, oppure richiedono manutenzione in marcia. Il core deve
respingerli, conservando il riferimento valido e le condizioni di processo.

Esempio di osservazione: errore scala encoder 1.25 con processo a 2 m/s.
Il sensore legge 2.5 m/s; il grafico deve mostrare entrambe le tracce con margine.
Rimuovendo la causa, il residuo rientra senza azzerare il tempo della prova.

La campagna automatica esegue 12 scenari su istanze indipendenti, con configurazione
e seed comuni: nominale, cambio riferimenti, nove anomalie e recupero del bias.
Il caso di cella bloccata cambia il riferimento di tensione dopo l'iniezione;
quello di derating applica una nuova rampa di velocità; il caso ritardo cambia
la tensione. I risultati possono comprendere fermate in guasto: l'esito non viene
forzato a successo. I report mostrano metriche e stato finale per ciascuna prova.

Allarmi del modello:

| Bit | Condizione | Conseguenza |
| --- | --- | --- |
| 1 | Encoder non valido | Frenatura memorizzata |
| 2 | Residuo velocità > max(0.08 m/s, 10% velocità reale) per 0.1 s | Avviso |
| 4 | Errore tensione > max(8 N, 15% riferimento) per 0.25 s | Avviso |
| 8 | Quota ballerino entro 0.1 mm dal finecorsa | Frenatura memorizzata |
| 16 | Saturazione continua per almeno 0.2 s | Avviso |
| 32 | Limite bobina/velocità/tensione del modello superato | Frenatura memorizzata |
| 64 | Materiale rotto | Frenatura, trasporto scollegato |
| 128 | Uscita controllore non valida | Callback disabilitata, frenatura memorizzata |

Le soglie sono scelte dimostrative, non tarature o protezioni certificate.
Il residuo encoder usa la verità del simulatore per analisi; non viene presentato
come una misura disponibile su qualunque linea reale. Un limite del modello non
viene convertito automaticamente in una rottura del materiale.

La forza dell'attuatore è firmata, positiva verso il basso. Il modello usa
un attuatore equivalente bidirezionale, senza circuito di pressione simulato.
La perdita moderata al 70% sposta il ballerino e non implica necessariamente
un allarme di finecorsa. Il verso della risposta dipende dal bilancio fra peso,
forza e tensione. Al limite di validità del modello (allarme 256), Nuova prova
reinizializza la simulazione; Conferma ripristino non supera quel limite.

Dal modello -4, i guasti forza e derating agiscono su azionamenti dinamici.
La perdita di forza ha una costante equivalente di 100 ms; le richieste di
coppia seguono una risposta di 20 ms, entro limiti di coppia e potenza.
L'attrito comprende componenti viscosa e Coulomb regolarizzata.
