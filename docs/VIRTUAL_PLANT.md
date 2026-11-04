<!-- SPDX-FileCopyrightText: 2026 Marco Pezzullo (Silicon LogiX)
SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0 -->
# Macchina virtuale di riferimento

ReelSync rappresenta una linea interamente simulata. Geometria, materiale,
azionamenti e sensori definiscono un impianto virtuale ripetibile. Il progetto
può essere sviluppato e verificato senza disporre di una linea reale.

## Materiali

| Profilo | Spessore | Larghezza | Modulo longitudinale | Densità |
| --- | --- | --- | --- | --- |
| Foglio elastico | 1 mm | configurabile | 60 MPa | 950 kg/m³ |
| Nastro elastico | 0,5 mm | configurabile | 120 MPa | 950 kg/m³ |
| PET di riferimento | 25 µm | 400 mm | 4,9 GPa | 1390 kg/m³ |
| PE-LD | 100 µm | 800 mm | 260 MPa | 923 kg/m³ |
| BOPP | 20 µm | 800 mm | 2,9 GPa | 900 kg/m³ |
| PA6 condizionato | 40 µm | 800 mm | 700 MPa | 1135 kg/m³ |

Per il PET, modulo e densità sono basati sui valori tipici pubblicati da
[Grafix Plastics per il film poliestere DuraLar](https://www.grafixplastics.com/materials-plastic-film-plastic-sheets/commodity-plastic-film/polyester-mylar-duralar-film/duralar-polyester-film-sheet-products/clear-duralar-polyester-film/):
710000 psi lungo la direzione macchina, arrotondati a 4,9 GPa, e 1,39 g/cm³.
Larghezza e spessore nominale sono scelte della linea virtuale. Il preset non
rappresenta ogni grado di PET o una qualifica del prodotto commerciale.

I profili sono selezionabili da **Prova e ricette** sul PC e da **Prove** sul
pannello. Fonti e condizioni: [MATERIALS.md](MATERIALS.md). Le ricette caricabili
sono incluse in `examples/recipes`.
Il limite EA ≤ 60000 N mantiene il dominio numerico dichiarato: per questo il
profilo PET imposta anche la larghezza. La scena adatta la larghezza del nastro.

I due profili elastici rimangono utili per osservare deformazioni e regolazione
con materiali più cedevoli. Le proprietà sono modificabili nella ricetta.

## Azionamenti e ballerino

| Grandezza | Valore del profilo |
| --- | --- |
| Risposta di coppia | Primo ordine, costante di tempo 20 ms |
| Zona a coppia costante | Fino a 8 rad/s sull’albero modellato |
| Potenza meccanica massima | Coppia massima configurata × 8 rad/s × quota disponibile |
| Valore nominale con 500 Nm | 4 kW per azionamento |
| Risposta equivalente di forza ballerino | Primo ordine, costante di tempo 100 ms |
| Attrito degli alberi | Viscoso 0,5 Nm·s + Coulomb regolarizzato 0,2 Nm |
| Attrito del traino | Viscoso 3 N·s/m + Coulomb regolarizzato 1 N |

Coppia e forza sono stati dinamici: dopo un comando non raggiungono
istantaneamente il valore richiesto. Sopra la velocità di base, la coppia
disponibile diminuisce per rispettare la potenza del profilo. Gli inviluppi
sono simmetrici in motorizzazione e frenatura; non si modella un bus DC.
Le condizioni iniziali includono le coppie di mantenimento della lastra tesa,
entro il limite configurato.

La rappresentazione mediante dinamica di primo ordine e inviluppo coppia/potenza
è usata anche nei modelli di sistema documentati da MathWorks:
[Mapped Motor](https://www.mathworks.com/help/autoblks/ref/mappedmotor.html) e
[Motor & Drive](https://www.mathworks.com/help/sdl/ref/motordrive.html).
Qui le costanti definiscono una macchina virtuale: non sono parametri
identificati su un azionamento specifico.

La forza equivalente del ballerino rimane separata dal peso e può agire nei
due versi. La perdita d’aria modifica la richiesta; la forza effettiva evolve
in 100 ms. Questo descrive una risposta equivalente di sistema. Pressioni,
portate e temperature nelle singole camere richiederebbero un modello
pneumatico dedicato e non vengono mostrate come valori calcolati.

L’attrito viscoso e il termine Coulomb smussato resistono al moto in entrambi
i versi. È una regolarizzazione continua, senza una legge di aderenza statica.
Riferimento per questa famiglia di modelli:
[Rotational Friction](https://www.mathworks.com/help/simscape/ref/rotationalfriction.html).

## Verifica senza macchina reale

La verifica combina soluzioni analitiche dei transitori, conservazione del
materiale, identità meccaniche, limiti di potenza, dissipazione dell’attrito,
confronto fra passi numerici, casi limite e replay deterministico. Tutti i
profili vengono provati con gli stessi scenari di guasto.
PC, scena 3D e dashboard consumano lo stesso stato della simulazione.

Questo consente di documentare il progetto come **simulatore di una linea
virtuale con parametri dichiarati**. La precisione rispetto a una particolare
macchina resta una proprietà distinta, che questo progetto non richiede.
