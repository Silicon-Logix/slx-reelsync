# SiliconLogiX ReelSync SDK 1.4.0 / ABI 1

Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
SPDX-License-Identifier: LicenseRef-SLX-ReelSync-SDK-Evaluation-1.0

Identità: SiliconLogiX, SLX ReelSync, `SLX_ReelSync_Core.dll`,
target CMake `SiliconLogiX::ReelSync`. Il manifest SDK riporta impronte e provenienza.
Gli export pubblici hanno prefisso `SLX_ReelSync_`. Solo tipi C a dimensione
esplicita, handle opaco e buffer del chiamante; nessuna STL o eccezione oltre ABI.

Ogni struttura versionata inizia con `uint32_t struct_size, abi_version`.
ABI 1 richiede dimensione esatta, versione 1 e allineamento naturale 8 byte.
Convenzione `cdecl` su Windows x64. Puntatori validi e dimensioni effettive dei
buffer sono responsabilità del chiamante C. Non usare packing personalizzato.
Il compilatore pubblico provato è LLVM-MinGW x64; l'import library fornita è GNU.

Sequenza: leggere evaluation.license come byte, Create, DefaultConfig, Configure,
SetController se didattico, SubmitCommand(Start), Step ripetuto, GetSnapshot e
GetPath. Destroy chiude l'handle con lo stesso modulo che lo ha creato.
Non riutilizzare un handle distrutto. Un handle va usato da un solo thread per
volta; istanze distinte sono indipendenti. Nessun thread o socket nel core.
GetSnapshot/GetPath eseguiti senza Step intermedio rappresentano lo stesso stato.

`Step` avanza esattamente il dt configurato solo in Marcia/Arresto/Guasto;
Pronto, Fermo, Manutenzione e Pausa mantengono il tempo. `SingleStep` richiede
Pausa e avanza un solo dt nello stato precedente. Pausa non è frenatura.
Il rendering usa un budget di passi con residuo conservato, mai dt variabile.
Un PC lento rallenta la presentazione temporale della prova senza cambiare
lo stato ottenuto a parità di passi, configurazione e comandi.

Comandi: id monotono non nullo per istanza; ripetizione dell'ultimo id restituisce
lo stesso esito senza riesecuzione; id più vecchi rifiutati. Payload del duplicato
non viene reinterpretato. Deadline positiva in tempo simulato, 0 senza scadenza.
Un solo autore dei comandi nel trasporto locale. Non è un protocollo Wi-Fi.
Le transizioni non ammesse ritornano errore e sono registrate.

Buffer path: prima interrogare required_count (BUFFER_ERROR), poi fornire capacità
sufficiente, massimo 512 punti. Coordinate già glTF Y-up (x,y,z) in metri,
lunghezza curvilinea in metri. La conversione originale non va ripetuta.
La geometria live deriva dallo stato e non usa i sei morph della presentazione.
Snapshot.phase_rad usa la convenzione nativa +Y della scena, mappata a -Z glTF;
idler e nip sono visualizzazioni cinematiche a inerzia trascurata del modello.

La callback didattica riceve dati temporanei, viene chiamata in modo sincrono
dentro Step, non deve bloccare, rilanciare eccezioni, richiamare l'handle o
conservare i puntatori. User data resta del chiamante fino a deregistrazione o
Destroy. Non è eseguita fuori processo: codice nativo di fiducia. Gli output
finiti sono limitati dal core a +/-0.3 m/s; NaN o ABI errata attivano un guasto
del controllore, disabilitano la callback e memorizzano una frenatura; richiedono una nuova prova.

Codici: 0 OK; 1 ingresso; 2 ABI; 3 stato; 4 licenza; 5 capability; 6 errore interno;
7 comando vecchio/scaduto; 8 buffer. Tutti gli ingressi numerici pubblici sono
controllati per finitezza e campo. `ReadEvents` legge senza consumare gli ultimi
256 eventi; sequence rivela eventuali lacune; l'host raccoglie periodicamente.

Da SDK 0.2.0, `GetDiagnostics` riempie una struttura ABI 1 separata senza
cambiare il layout degli snapshot precedenti: allarmi attivi e memorizzati,
stato precedente alla pausa, intensità/età della causa, residuo encoder in m/s
e utilizzo della coppia disponibile (0..1). Qualità dei segnali nello snapshot:
0 non valida, 1 nominale, 2 alterata da un'anomalia dimostrativa.

Comandi aggiunti: `RemoveFault` (13) rimuove una causa reversibile senza
azzerare tempo o stato; `Acknowledge` (14) conferma allarmi rientrati, e una
fermata solo a linea stabilizzata. Rottura e callback non valida richiedono
nuova prova. Una causa attiva per volta, con più allarmi conseguenti possibili.

Da SDK 0.3.0, `GetWebTracking` riempie `SLX_ReelSync_WebTracking` ABI 1.
`material_origin_m` e la coordinata materiale al distacco dello svolgitore;
coincide con la lunghezza a riposo ancora nella bobina di svolgimento.
`upstream_rest_m` e `downstream_rest_m` sono gli inventari a riposo nelle due
zone. `upstream_route_m` e `downstream_route_m` dividono il percorso visuale
al centro dell'avvolgimento sul drive roller; includono il distacco visuale
di 0.4 mm, coerente con `GetPath`. `connected` vale 0 dopo una rottura.

Per interpolare il disegno sulla lastra, distribuire gli inventari lungo le
rispettive zone geometriche. In un punto a distanza curvilinea s nella prima
zona, la coordinata e `origin + upstream_rest * s / upstream_route`;
nella seconda, `origin + upstream_rest + downstream_rest * (s-upstream_route) /
downstream_route`. Ridurre la parte intera del periodo prima della conversione
in float mantiene la precisione visuale. Questi dati sono di sola lettura e
restano invariati se il core e in pausa. Non costituiscono un sensore di registro.
Soglie, parametri e vincoli sono in FAULT_LAB.md. Il limite di attrito usa ora
il fattore inviato in `value` (1..10); gli altri parametri sono validati nel core.

La pianificazione è un servizio dell'host sopra gli stessi comandi SDK:
inizio e durata sono in tempo simulato, limite 32 azioni. Il core conferma
o respinge ciascuna azione. Gli eventi esportati registrano anche la rimozione;
il replay usa i comandi effettivamente accettati, non rischedula il piano.
L'host raccoglie gli eventi almeno a ogni campione di registrazione e alla
fine di ogni batch. Eventi 105: `result` è la maschera degli allarmi attivi,
non un codice di errore SDK; `command_id=0` indica evento generato dal core.

Il modello 1.4.0 usa l'identità elastic-two-zone-4. Il launcher verifica
versione core e ABI prima di caricare l'host. Un replay richiede anche build ID
e impronta del modulo C corrispondenti; non è una conversione tra versioni.

Capability nel core: simulazione, callback didattica, guasti e percorso live.
Il profilo offline di valutazione è fisso nel core e richiede il documento byte
per byte. Non è un entitlement crittografico: il testo non è un segreto e non
impedisce reverse engineering o riuso abusivo. Nessuna firma, attivazione cloud,
chiave privata o protezione assoluta viene dichiarata. La tutela combina sorgenti
privati, export limitati, inventario e condizioni di utilizzo in `SDK_LICENSE`.

Da SDK 1.0.0, `GetMechanicalReadings` riempie la struttura separata
`SLX_ReelSync_MechanicalReadings` ABI 1: forza firmata dell'attuatore in N, positiva verso il basso,
quota disponibile 0..1, lunghezze lasche delle due zone e potenze meccaniche
con segno di svolgitore, riavvolgitore e nip. La forza applica la stessa legge
del modello; non è una pressione o una misura di consumo elettrico. Il getter
è di sola lettura e non modifica i layout degli snapshot precedenti.

`Transport`/`Session` nel namespace `slx::reelsync` costituiscono l'adattatore
locale del modello applicativo. Snapshot, eventi e decoder dei comandi sono
descritti in [TELEMETRY.md](TELEMETRY.md). Trasporto, autenticazione,
riconnessioni e autorità multiple restano nella fase distribuita.

`Session::recording()` e `restore()` conservano e ricostruiscono una prova
tramite configurazione, seed e comandi accettati: nessuna serializzazione dello
stato fisico privato. Build, modello e impronta del modulo C devono coincidere;
la verifica fallita lascia intatta la sessione esistente. Il ripristino conserva
anche comandi respinti, identificativo della prova e azioni future pianificate.

Dalla 1.4.0 il modello è `elastic-two-zone-4`: peso del ballerino esplicito,
forza attuatore firmata (positiva verso il basso, negativa verso l'alto), limite
di validità memorizzato in allarme 256/evento 106. I layout ABI 1 restano invariati.
Le prove registrate con modello -2 vanno riaperte nella versione originaria:
non si dichiara replay identico fra modelli fisici diversi.

Il modello -4 aggiunge quattro stati interni per coppie effettive dei tre
azionamenti e forza attuatore. Lo snapshot e MechanicalReadings riportano le
grandezze effettive. La quota pneumatica disponibile è la quota impostata dal
guasto, quindi non coincide con il rapporto di forza durante il transitorio.
Il limite di potenza meccanica è 8 rad/s moltiplicato per il limite di coppia
configurato e per il derating. Layout ABI ed export restano invariati.

## Disponibilita del ripristino (1.5.0)

`SLX_ReelSync_Diagnostics.command_flags` usa lo slot ABI 1 precedentemente riservato, mantenuto anche con alias `reserved`. Dimensione e allineamento restano invariati. `SLX_RS_DIAG_CAN_ACKNOWLEDGE` indica che il core ammette ACKNOWLEDGE nello stato corrente, comprese le condizioni di arresto. Il comando continua a validare lo stato al momento della ricezione. Gli altri bit sono riservati. Il campo non concede autorita di controllo.
