// SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#pragma once
#include <array>
#include <slx/reelsync.h>
namespace slx::reelsync {
struct FaultSpec {
  uint32_t id;
  const char *name;
  const char *description;
  const char *unit;
  double initial, min, max, step;
  bool reversible;
};
inline constexpr std::array<FaultSpec, 9> faultCatalog{
    {{1, "Perdita encoder", "Lettura velocità congelata, qualità non valida e arresto in guasto.",
      "", 0, 0, 0, 0, true},
     {2, "Bias cella di carico",
      "Offset sulla tensione misurata: il regolatore reagisce a una misura errata.", "N", 10, -20,
      20, 1, true},
     {3, "Attrito azionamenti",
      "Aumento dell'attrito viscoso nei due avvolgitori e nel trascinamento.", "x", 5, 1, 10, .5,
      true},
     {4, "Rottura del materiale",
      "Tensione e trasporto disconnessi; frammenti conservati nel bilancio.", "", 0, 0, 0, 0,
      false},
     {5, "Derating degli azionamenti",
      "Riduzione della coppia disponibile; possibili saturazioni durante le rampe.",
      "quota disponibile", .15, .1, 1, .05, true},
     {6, "Perdita forza pneumatica",
      "Riduzione del precarico del ballerino: la posizione segue le forze del modello.",
      "quota residua", .7, 0, 1, .05, true},
     {7, "Errore scala encoder",
      "Fattore di calibrazione errato sulla velocità letta, con allarme di discordanza.", "fattore",
      1.25, .5, 1.5, .05, true},
     {8, "Cella di carico bloccata",
      "Mantiene l'ultima lettura; un cambio riferimento rivela la perdita di feedback.", "", 0, 0,
      0, 0, true},
     {9, "Ritardo dei sensori", "Ritardo addizionale su tensione e velocità, in multipli di 5 ms.",
      "s", .1, .005, .2, .005, true}}};
struct ScenarioSpec {
  const char *id;
  const char *name;
  const char *description;
  uint32_t fault;
  double severity;
  bool recover;
};
inline constexpr std::array<ScenarioSpec, 12> scenarioCatalog{
    {{"ramp", "Rampa nominale", "Avvio a 2 m/s, bobine variabili e arresto a 45 s.", 0, 0, false},
     {"setpoint", "Cambio riferimenti", "A 10 s: 3 m/s e 100 N; arresto controllato a 45 s.", 0, 0,
      false},
     {"encoder", "Perdita encoder",
      "Guasto a 20 s; osservazione della frenatura e della misura non valida.", 1, 0, false},
     {"bias", "Bias tensione", "Offset +10 N a 20 s: confronto fra misura e tensione reale.", 2, 10,
      false},
     {"friction", "Attrito anomalo", "Attrito x5 a 20 s: risposta di coppie, tensione e ballerino.",
      3, 5, false},
     {"break", "Rottura materiale",
      "Rottura a 20 s; tensioni nulle e frammenti trattenuti nel modello.", 4, 0, false},
     {"drive", "Coppia insufficiente", "Coppia al 15% a 20 s e rampa aggiuntiva a 24 s.", 5, .15,
      false},
     {"air", "Calo pneumatico", "Forza attuatore al 70% a 20 s; spostamento del ballerino.", 6, .7,
      false},
     {"encoder-scale", "Calibrazione encoder",
      "Scala encoder 1.25 a 20 s: residuo fra lettura e processo.", 7, 1.25, false},
     {"stuck", "Misura bloccata", "Cella bloccata a 20 s, tensione richiesta a 100 N a 24 s.", 8, 0,
      false},
     {"lag", "Feedback ritardato", "100 ms aggiuntivi a 20 s, cambio tensione a 24 s.", 9, .1,
      false},
     {"recovery", "Anomalia e ripristino",
      "Bias +10 N a 20 s, rimozione a 30 s senza azzerare la prova.", 2, 10, true}}};
inline const char *faultName(uint32_t id) {
  return id >= 1 && id <= faultCatalog.size() ? faultCatalog[id - 1].name : "Nessuna anomalia";
}
inline constexpr std::array<const char *, 9> alarmNames{
    {"Encoder non valido", "Discordanza velocità", "Scostamento tensione", "Finecorsa ballerino",
     "Coppia satura persistente", "Limite operativo", "Materiale rotto", "Controllore non valido",
     "Validità del modello superata"}};
} // namespace slx::reelsync
