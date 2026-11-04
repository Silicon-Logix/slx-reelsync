// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
// Copyright (c) 2026 Marco Pezzullo / Silicon LogiX. See LICENSE.
#pragma once
#include "faults.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace slx::reelsync {
struct AlarmSpec {
  const char *code, *component, *action;
};
inline constexpr std::array<AlarmSpec, 9> alarmCatalog{
    {{"E001", "Encoder traino", "Verificare segnale encoder e collegamento; attendere l'arresto."},
     {"E002", "Traino / encoder",
      "Confrontare velocita letta e velocita del processo; verificare la scala encoder."},
     {"E003", "Cella di carico",
      "Verificare tensione, riferimento e misura della cella di carico."},
     {"E004", "Ballerino", "Verificare posizione ballerino, precarico e tensione della lastra."},
     {"E005", "Azionamenti", "Verificare carico, attrito, rampa e coppia disponibile."},
     {"E006", "Linea", "Limite operativo raggiunto: verificare velocita, tensione e bobine."},
     {"E007", "Lastra", "Materiale interrotto: attendere l'arresto e preparare una nuova prova."},
     {"E008", "Controllo", "Controllore non valido: arrestare e verificare la configurazione."},
     {"E009", "Modello", "Dominio di validita superato: avviare una nuova prova."}}};
inline unsigned alarmCount(uint32_t mask) {
  unsigned n = 0;
  for (; mask; mask &= mask - 1)
    ++n;
  return n;
}
inline std::vector<std::string> attentionParts(uint32_t fault, uint32_t mask) {
  std::vector<std::string> parts;
  auto add = [&](const char *s) { parts.emplace_back(s); };
  if (fault == 1 || fault == 7 || (mask & 3))
    add("encoder");
  if (fault == 2 || fault == 8 || (mask & 4))
    add("loadcell");
  if (fault == 3 || fault == 5 || (mask & 16))
    add("drives");
  if (fault == 4 || (mask & 64))
    add("web");
  if (fault == 6 || (mask & 8))
    add("dancer");
  if (fault == 9) {
    add("encoder");
    add("loadcell");
  }
  if (mask & (32 | 128 | 256))
    add("line");
  return parts;
}
inline bool partMatches(const std::string &part, const std::string &node) {
  if (part == "encoder")
    return node == "rs_drive_roller_servo" || node == "rs_drive_module_root";
  if (part == "loadcell")
    return node == "rs_loadcell_roller" || node == "rs_loadcell_roller_front_bearing" ||
           node == "rs_loadcell_roller_rear_bearing";
  if (part == "drives")
    return node == "rs_unwinder_servo" || node == "rs_rewinder_servo" ||
           node == "rs_drive_roller_servo";
  if (part == "dancer")
    return node == "rs_dancer_module_root" || node == "rs_pneumatic_service_root";
  if (part == "web")
    return node == "RS_WebFilm" || node == "rs_nip_module_root";
  return part == "line" && (node == "rs_drive_module_root" || node == "rs_dancer_module_root");
}
} // namespace slx::reelsync
