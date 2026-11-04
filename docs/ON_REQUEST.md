<!-- SPDX-FileCopyrightText: 2026 Marco Pezzullo (Silicon LogiX)
SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0 -->
# Sviluppi su richiesta

Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).

Ricerca tecnica consultata il 2 novembre 2026. Le fonti descrivono applicazioni
industriali; le proposte ReelSync qui sotto sono valutazioni progettuali nostre.
Non implicano integrazioni gia disponibili, affiliazione o compatibilita certificata.
I collegamenti nella pagina dell'applicazione si aprono solo su azione dell'utente.

| Priorita proposta | Estensione | Obiettivo e dati necessari | Stato |
| --- | --- | --- | --- |
| 1 | Taper tension | Confrontare curve di riduzione della tensione al crescere del diametro. Servono curve di ricetta e misure reali. Una previsione dei difetti richiede un modello di bobina dedicato. | Da sviluppare |
| 2 | Guida bordo e registro | Studiare deriva laterale, attuatore e perdita del riferimento. Servono geometria, sensori e prestazioni dell'attuatore. | Da sviluppare, nuova dinamica laterale |
| 3 | Ispezione e mappa difetti | Collocare difetti sintetici lungo il materiale e ricostruire la bobina. Per una telecamera reale servono ottica, illuminazione e campioni etichettati. | Da sviluppare |
| 4 | Virtual commissioning | Collegare un PLC simulato o un banco di prova per sequenze e guasti. Servono programma, mappa I/O, tempi di ciclo e criteri di accettazione. | Da sviluppare, nessun adapter PLC nella demo |
| 5 | Albero differenziale e pressione | Estendere il modello a piu bobine, frizioni e pressione. Servono specifiche, inerzie e curva pressione-coppia. | Da sviluppare, nuova meccanica e calibrazione |

La priorita considera vicinanza al modello attuale e lavoro di integrazione;
non e una stima di costo, disponibilita o ritorno economico.

## Motivazione e fonti primarie

DFE descrive il controllo a zone e la riduzione dell'uscita del controllore durante
la crescita della bobina. Questo motiva il confronto tra tensione costante e taper,
ma non rende automaticamente predittivo il modello elastico attuale.
[DFE: Tension Control By Zone](https://dfe.com/applications/tension-control-by-zone/).

Erhardt+Leimer documenta sistemi di guida del materiale, inclusi telai e rulli
orientabili. Una simulazione della correzione laterale necessita di una dinamica
aggiuntiva rispetto al percorso longitudinale ReelSync.
[E+L: Web guiding systems](https://www.erhardt-leimer.com/us/en/products/web-guiding-technology/web-guiding-systems.html).

E+L descrive inoltre l'ispezione di film e carta con camere lineari e sottolinea
il ruolo dell'illuminazione nel rendere osservabili i difetti. La mappa sintetica
e una possibile prima estensione; l'accuratezza ottica va verificata separatamente.
[E+L: ELSIS surface inspection](https://www.erhardt-leimer.com/us/en/products/inspection-technology/elsis-surface-inspection.html).

Il manuale Siemens mostra l'accoppiamento tra controllore simulato, modello
comportamentale e scena, anche per studiare anomalie. E un riferimento per
definire un futuro confine I/O e prove SIL/HIL, senza dichiarare un'integrazione
ReelSync gia realizzata.
[Siemens: Virtual Commissioning, V1.0.1, marzo 2024](https://cache.industry.siemens.com/dl/files/943/109758943/att_1265915/v1/Manual_VirtualCommissioningOfMachines_GettingStarted_V1.0.1_EN.pdf).

La brochure Tidland illustra controllo dell'albero differenziale tramite pressione,
feedback di tensione e misura del diametro. Questo suggerisce un'estensione
meccanica specialistica, ulteriore rispetto alle due bobine del modello attuale.
[Maxcess/Tidland: Winding Solutions, pagina 8 del PDF](https://www.maxcessintl.com/wp-content/uploads/2021/06/2021-Tidland-Winding-Solutions-Brochure.pdf).

## Strumenti gia realizzati nella 0.3.0

In Regolazione si arma l'arresto a metratura: 0 lo disattiva; l'obiettivo e riferito
ai metri passati al nip dall'inizio della prova, non alla lunghezza finale della
bobina ne a una misura di registro. L'host anticipa l'arresto con la distanza
nominale di frenata della rampa configurata. Cambi di riferimento, saturazioni o
guasti possono produrre uno scarto: il report salva l'errore finale effettivo.

La pausa al primo allarme e un trigger di analisi a scatto singolo. Congela tempo
e scena; non e un arresto fisico. Dopo il trigger, disattivare e riattivare la casella
per riarmarlo. Nuova prova e nuova configurazione azzerano questi strumenti.

Il JSON salva obiettivo, trigger ed errore finale in `operator_tools`. I comandi
Stop/Pause accettati sono registrati negli eventi. Il replay li riproduce senza
riarmare le automazioni, evitando doppie esecuzioni.
