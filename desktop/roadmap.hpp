// Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#pragma once
#include <array>
namespace slx::reelsync {
struct Extension {
  const char *name, *benefit, *scope, *inputs, *source, *url;
};
inline constexpr std::array<Extension, 5> extensions{
    {{"Taper tension e qualita della bobina",
      "Studiare la riduzione della tensione al crescere del diametro.",
      "Curve lineari o per ricetta, confronto con tensione costante e analisi delle transizioni. "
      "La previsione dei difetti richiede un modello dedicato.",
      "Materiale, diametri, curva di tensione e misure sulla bobina reale.",
      "Dover Flexo Electronics / Tension control by zone",
      "https://dfe.com/applications/tension-control-by-zone/"},
     {"Guida bordo e registro",
      "Valutare deriva laterale, sensori e correzione della posizione del materiale.",
      "Dinamica laterale, attuatore guidanastro, sensori di bordo, perdita del riferimento e "
      "ritardi.",
      "Geometria guidanastro, caratteristiche sensori, corsa e velocita attuatore.",
      "Erhardt+Leimer / Web guiding systems",
      "https://www.erhardt-leimer.com/us/en/products/web-guiding-technology/"
      "web-guiding-systems.html"},
     {"Ispezione e mappa dei difetti", "Associare difetti e scarti alla posizione lungo la bobina.",
      "Difetti sintetici con coordinate materiale, mappa bobina ed eventi di ispezione. Una "
      "telecamera reale e l'accuratezza di rilevamento richiedono integrazione e validazione.",
      "Tipi e dimensioni dei difetti, risoluzione ottica, immagini e campioni etichettati.",
      "Erhardt+Leimer / ELSIS surface inspection",
      "https://www.erhardt-leimer.com/us/en/products/inspection-technology/"
      "elsis-surface-inspection.html"},
     {"Avvolgimento differenziale e pressione",
      "Confrontare tensione, diametro e carico nell'avvolgimento di piu bobine.",
      "Modello di albero differenziale, frizioni e pressione regolata, con estensione della scena "
      "e calibrazione su misure reali.",
      "Specifiche dell'albero, inerzie, attriti e curva pressione-coppia.",
      "Maxcess / Tidland Winding Solutions",
      "https://www.maxcessintl.com/wp-content/uploads/2021/06/"
      "2021-Tidland-Winding-Solutions-Brochure.pdf"},
     {"Virtual commissioning e formazione PLC",
      "Provare sequenze, interblocchi e guasti prima della messa in servizio.",
      "Adapter PLC, mappa I/O, modalità SIL/HIL e scenari di accettazione personalizzati.",
      "PLC e protocollo scelti, tempi di ciclo, programma, mappa I/O e criteri di prova.",
      "Siemens / Virtual Commissioning of Machines",
      "https://cache.industry.siemens.com/dl/files/943/109758943/att_1265915/v1/"
      "Manual_VirtualCommissioningOfMachines_GettingStarted_V1.0.1_EN.pdf"}}};
} // namespace slx::reelsync
