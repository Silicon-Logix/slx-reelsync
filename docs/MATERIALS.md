<!-- SPDX-FileCopyrightText: 2026 Marco Pezzullo (Silicon LogiX)
SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0 -->
# Materiali della linea virtuale

Il catalogo offre sei profili. La selezione applica le quattro proprietà del
materiale; velocità, tensione e impostazioni della macchina rimangono configurabili.
PC e Raspberry usano lo stesso catalogo trasmesso dal simulatore.

| Profilo | Spessore | Larghezza | Modulo longitudinale | Densità |
| --- | --- | --- | --- | --- |
| Elastico 1,0 mm | 1000 µm | 800 mm | 60 MPa | 950 kg/m³ |
| Elastico 0,5 mm | 500 µm | 800 mm | 120 MPa | 950 kg/m³ |
| PET | 25 µm | 400 mm | 4900 MPa | 1390 kg/m³ |
| PE-LD | 100 µm | 800 mm | 260 MPa | 923 kg/m³ |
| BOPP | 20 µm | 800 mm | 2900 MPa | 900 kg/m³ |
| PA6 condizionato | 40 µm | 800 mm | 700 MPa | 1135 kg/m³ |

PET: valori tipici del film poliestere nella scheda
[Grafix Plastics](https://www.grafixplastics.com/materials-plastic-film-plastic-sheets/commodity-plastic-film/polyester-mylar-duralar-film/duralar-polyester-film-sheet-products/clear-duralar-polyester-film/),
con modulo MD arrotondato da 710000 psi. PE-LD: modulo e densità del grado per
film [Lupolen 2420F, LyondellBasell](https://www.lyondellbasell.com/en/polymers/p/Lupolen-2420F/61e46490-ed79-4c71-bd7a-35125886bb3a).

BOPP: valori del film orientato 20 µm riportati nella
[scheda LyondellBasell Moplen HP522H](https://hongrunplastics.com/public/uploads/images/20250609/LyondellBasell%20PP%20Moplen%20HP522H.pdf).
Si usa il modulo di trazione del film, 2900 MPa, nelle condizioni della scheda.
PA6: film cast condizionato Ultramid B36 LN 01, tabella 2 della
[guida BASF per estrusione](https://chemicals.basf.com/dam/jcr:794caec6-5ccb-4345-8cc5-26f39840adca/BASF_Ultramid_Properties_and_Extrusion_Processing_Guide.pdf),
modulo MD 700 MPa e densità rappresentativa 1135 kg/m³, media dell’intervallo
1120–1150 kg/m³ pubblicato.

Le larghezze e gli spessori dei profili definiscono la macchina virtuale. I due
materiali elastici sono riferimenti ingegneristici configurabili. Le proprietà
pubblicate sono valori tipici, dipendenti da lavorazione e condizionamento.
Ogni profilo usa la legge elastica longitudinale del modello; umidità,
temperatura, plasticizzazione e viscoelasticità non evolvono durante la prova.

## Selezione e personalizzazione

Sul PC aprire **Prova e ricette → Materiale**. Sulla Raspberry aprire
**Prove → Scegli materiale**, consultare dimensioni e proprietà e confermare
la selezione a linea ferma. Il PC archivia la prova precedente.

I parametri restano modificabili sul PC. Una combinazione modificata viene
identificata come **Personalizzato**. È possibile salvare e ricaricare la ricetta.
Se larghezza, spessore e modulo superano il dominio numerico ammesso, la
configurazione viene respinta prima di modificare la prova attiva.

Le ricette di esempio sono in `examples/recipes`. Il catalogo completo con
proprietà e provenienza è in [material-catalog.json](material-catalog.json).
La CLI lo esporta con `--list-materials`. `trial.json` conserva materiale e
configurazione, così i risultati mantengono il riferimento al profilo utilizzato.
