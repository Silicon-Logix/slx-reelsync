# Modello ridotto elastic-two-zone-4

Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0

Unità SI: lunghezze m, tempo s, massa kg, forza N, coppia Nm, modulo Pa,
inerzia kg m², angoli rad. Coordinate native Z-up, trasporto +X, alberi Y;
il GLB è già Y-up, (x,y,z) nativo diventa (x,z,-y).

Due zone longitudinali uniformi, divise al centro dell'avvolgimento sul rullo
di trascinamento: svolgitore→nip e nip→riavvolgitore (comprende ballerino).
Percorso con tangenti esatte fra i nove cerchi dei contratti e archi minori.
Le lunghezze geometriche L_u,L_r dipendono dai raggi e dalla quota z del ballerino.
La superficie visiva aggiunge 0.4 mm di distacco radiale; non altera la lunghezza
fisica. Archi 3D discretizzati in 48 corde; nessun morph dimostrativo usato live.
Il renderer usa PBR semplificato con materiali/texture originali e blending
alpha delle protezioni; non calcola rifrazione fisica del policarbonato.

Ogni zona contiene una lunghezza a riposo N. Deformazione e tensione:

```text
A = larghezza * spessore
epsilon_i = L_i / N_i - 1
T_i = E A max(epsilon_i, 0)
R_i = sqrt(r_core² + h C_i / pi)
m_i = rho A C_i
J_i = J_hub + m_i (R_i² + r_core²)/2
q_u = omega_u R_u / (1 + T_u/(EA))
q_r = omega_r R_r / (1 + T_r/(EA))
q_d = v / (1 + T_donor/(EA))
C_u'=-q_u, C_r'=q_r, N_u'=q_u-q_d, N_r'=q_d-q_r
```

C sono lunghezze avvolte non deformate; q flussi di lunghezza a riposo;
donor è la zona a monte secondo il verso del trasporto. Pertanto
C_u+C_r+N_u+N_r è invariante. Il materiale sulla bobina si considera rilassato;
nessuna compattazione. Per epsilon<0 la tensione è nulla: il modello conserva
la lunghezza lasca, mentre il 3D mostra il percorso guida nominale, senza catenaria
o pieghe. La mesh non rappresenta la distribuzione dello slack.

Gli avvolgitori sono volumi di controllo aperti, materiale entra/esce alla
velocità periferica; il flusso di momento angolare è coerente con la variazione
di J del guscio, evitando l'aggiunta artificiale di un termine J' omega.
Rulli passivi ideali, attrito e inerzia delle guide trascurati; nip senza slip.
Gli azionamenti hanno dinamica di coppia e inviluppo coppia/potenza del profilo
della macchina virtuale, descritto in VIRTUAL_PLANT.md.

```text
J_u omega_u' = tau_u + T_u R_u - drag_u(omega_u)
J_r omega_r' = tau_r - T_r R_r - drag_r(omega_r)
M_nip v' = F_nip - T_u + T_r - drag_nip(v)
F_goal = quota_aria (-T_ref (dL_r/dz a z=1.20) - m_d g)
F_act' = (F_goal - F_act) / 0.10 s
tau_i' = (tau_demand_i - tau_i) / 0.020 s
m_d z'' = -T_r dL_r/dz - F_act - m_d g - k(z-1.20) - c z'
```

Z positivo verso l'alto. La geometria produce dL_r/dz<0; la tensione solleva il
ballerino. Attuatore equivalente bidirezionale: F_act positiva verso il basso,
negativa verso l'alto. La richiesta a quota centrale bilancia tensione e peso, con g=9.80665
m/s². In condizioni nominali T=80 N e m=8 kg, F_act=66.11 N e peso=78.45 N.
Perdita aria riduce la richiesta di forza; la forza effettiva evolve con
una costante di tempo di 100 ms. Il peso rimane presente.
Con massa elevata e tensione bassa l'attuatore sostiene il ballerino; perdendo
forza questo scende. Il profilo descrive una risposta equivalente di forza; pressioni delle camere,
portate e temperature del circuito pneumatico non sono stati del modello.
Il quadrante 3D mostra la forza firmata dell'attuatore in N, campo -400..+400.
M_nip=20 kg equivalenti, J_hub=1.5 kg m²,
Attrito nominale alberi: 0.5 omega + 0.2 tanh(omega/0.02) Nm.
Attrito nip: 3 v + tanh(v/0.01) N. Le forze resistono al moto; il guasto
attrito moltiplica entrambe le componenti. k=250 N/m, c=100 N s/m. Arresti di corsa
con impatto perfettamente anelastico: proiezione della posizione al limite e
annullamento della sola velocità penetrante. Nessuna conservazione energetica
all'impatto viene dichiarata.

Impianto e quattro stati degli attuatori integrati con RK4 a passo fisso;
richieste del controllo mantenute costanti nei quattro stadi e aggiornate a ogni passo. Non si dichiara ordine quattro per il sistema chiuso
con controllore campionato. Default dt=0.5 ms, ammessi 0.125..1 ms se divisori
interi di 5 ms. Limiti della rigidezza EA<=60000 N e massa ballerino>=5 kg;
nessuna garanzia universale di stabilità per tutte le combinazioni ammesse.
Controllare convergenza e risultato della singola ricetta prima di trarre conclusioni.

Regolazione di riferimento con compensazione raggio/inerzia, rampe, feedback e
anti-windup condizionale; coppie limitate nel core. Modulo didattico pubblico P
produce un trim di velocità, senza accesso agli oggetti fisici interni. Confronto
sempre con la stessa configurazione e sequenza comandi; coefficienti del modulo
e versione vanno aggiornati insieme quando si modifica l'esempio.

Sensori a 200 Hz: velocità encoder e tensione a valle; rumore uniforme con
xorshift64*, seed registrato, bias additivo e ritardo FIFO a multipli di 5 ms.
Il feedback usa la tensione misurata; i diametri e la posizione esposti sono
sensori ideali ricavati dallo stato, senza rumore configurabile in questa versione.
Tensione a monte ideale per il controllore. Nessuna misura di corrente o potenza
elettrica. Le potenze meccaniche esposte sono prodotti coppia per velocità angolare;
per il nip si usa la forza equivalente per la velocità periferica.

Guasti: perdita encoder mantiene l'ultima misura e qualità invalida, con frenatura;
bias tensione altera realmente il feedback; attrito moltiplica per 5 i tre
coefficienti dissipativi. Rottura azzera tensioni e flussi: i frammenti rimangono
nei rispettivi serbatoi di materiale, motori frenano e ballerino scende al finecorsa.
Nessuna meccanica di frattura, caduta o raccolta dei frammenti. Superficie nastro
nascosta nel 3D rotto. Superamento inviluppo bobina/velocità o tensione>2000 N
attiva una frenatura memorizzata, senza convertire il limite in una rottura.

In 0.2.0 l'attrito è selezionabile nel campo 1..10 (preset x5). Derating degli
azionamenti: tutte le coppie vengono limitate alla quota disponibile del limite
nominale. Perdita pneumatica: la richiesta di forza viene moltiplicata per la quota residua;
non si modifica una pressione inventata. L'errore scala encoder moltiplica la
misura, mentre la cella bloccata trattiene l'ultima lettura. Il ritardo aggiuntivo
seleziona campioni dalla storia a 200 Hz, fino a 0.25 s complessivi con il ritardo
di ricetta. Il ripristino reversibile non ricrea le bobine né cancella il tempo.
Per allarmi e prove vedere FAULT_LAB.md; le soglie sono scelte dimostrative.

Campo visuale ammesso: larghezza 0.2..0.8 m, nucleo 0.10 m, raggi iniziali U
0.15..0.62 m, R 0.15..0.60 m; runtime massimo 0.62 m, corsa 1.05..1.35 m.
Le bobine deformano l'esterno preservando il foro; la sola larghezza assiale
del materiale viene adattata. Nessuna scala globale di alberi o macchina.
Gli ingombri dei contratti sono vincoli visuali; non un progetto costruttivo.

Ricette: elastico generico 1 mm, E=60 MPa, rho=950 kg/m³ e variante 0.5 mm,
E=120 MPa. Sono esempi di rigidezza equivalente, non caratterizzazioni PET
certificate. Non descrivono isteresi, viscoelasticità, multistrato, slip, usura,
pressione di avvolgimento, instabilità laterale o contatto FEM. Nessun confronto
con misure di una linea reale è stato eseguito.

Registrazione a 50 Hz. Bilancio richiesto nei test standard <1e-7 m; convergenza
fra 1 ms e 0.25 ms: differenza tensione<0.5 N e quota<2 mm nei casi dichiarati
nel report interno. Queste soglie verificano la numerica, non l'accuratezza fisica.

Il nip ha raggio 0.14 m: la sua fase integra la velocità periferica divisa
per tale raggio. Le fasi delle guide passive sono riferimenti cinematici
ideali relativi al nip, corretti per la variazione del percorso; non misure
encoder indipendenti. Alla rottura rimangono ferme senza salto, mentre motori e
bobine seguono la propria frenatura. L'HMI riceve dal PC geometria, fasi e
coordinate materiali; la Raspberry interpola i dati senza integrare un secondo
modello. A Z crescente il ballerino sale anche sul pannello.

La riserva nominale di arresto usa 1 m + v_max²/(2 a_ricetta), con il massimo
delle velocità periferiche positive dei tre azionamenti. Esaurimento o capacità
prossima della bobina attivano frenatura memorizzata. La riserva non garantisce
l'arresto sotto saturazione: un ulteriore controllo impedisce di accettare un
passo con inventari negativi, zone a riposo <=0.1 m, raggio oltre 0.62 m o
stato non finito. Mantiene l'ultimo stato valido, emette evento 106 e allarme
256, sospende la prova e richiede Nuova prova. Questo è un limite di validità
della simulazione, distinto da un arresto fisico riuscito.

Dalla 1.3.0 è disponibile anche PET 25 um, larghezza 400 mm, E=4.9 GPa e
rho=1390 kg/m³. Modulo e densità provengono da valori tipici pubblicati per
film poliestere; larghezza e spessore nominali definiscono il profilo virtuale.
Il campo di spessore è 20..1000 um, sempre entro EA<=60000 N.
Le prove analitiche e numeriche sono il criterio di verifica del progetto
interamente simulato. La disponibilità di una linea reale non è un requisito.
