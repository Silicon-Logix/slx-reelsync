<!-- SPDX-FileCopyrightText: 2026 Marco Pezzullo (Silicon LogiX)
SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0 -->
# SLX ReelSync 1.5.0 / Dashboard 1.6.0

La dashboard si apre in profilo **Operatore**: conduzione della linea,
regolazione, ricette, arresto a metratura, grafici, allarmi ed eventi.
Iniezione delle anomalie, pausa analisi, singolo passo e campagne di prova
sono disponibili solo attivando **Sistema → Display → Abilita laboratorio**.
Al riavvio il pannello torna al profilo Operatore.

L'acquisizione del controllo e i comandi riusciti aggiornano lo stato senza
aprire finestre. Restano le conferme per le operazioni che modificano la
configurazione e le finestre che richiedono di correggere un errore.

Gli allarmi rilevati dal core hanno codice, nome, componente e indicazione
operativa. La barra globale e i richiami nelle pagine Allarmi/Eventi diventano
rossi quando esiste un allarme attivo. Le condizioni rientrate restano in
memoria, in ambra, fino alla conferma del ripristino. Questa è disponibile
solo quando il core conferma anche le condizioni di arresto necessarie.

Il simulatore evidenzia il componente nel 3D; la dashboard evidenzia la zona
corrispondente nello schema. Una causa introdotta per la prova è distinta
dagli allarmi rilevati: l'indicazione **Prova simulatore** non inventa una
diagnosi prima del superamento delle condizioni di allarme.

L'iniezione dispone di un conto alla rovescia predefinito di **10 secondi di
prova**, visibile su entrambe le interfacce e annullabile. Il simulatore
torna alla vista 3D dopo la preparazione. La pausa congela anche il timer;
il moltiplicatore del ritmo modifica il rapporto con il tempo reale.
La preparazione/rimozione di una causa dal laboratorio PC conserva il
controllo del pannello. Un comando locale di conduzione lo riprende.

Il modello fisico resta `elastic-two-zone-4`; i sei materiali sono gli stessi.
ABI C 1 e 15 funzioni esportate restano invariati. Il campo riservato della
diagnostica ha un alias `command_flags`, con il bit di disponibilità della
conferma del ripristino.
