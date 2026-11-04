// Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#include "protocol.hpp"
#include "faults.hpp"
#include <charconv>
#include <cmath>
#include <map>
#include <set>
namespace slx::reelsync {
SLX_ReelSync_Command decodeCommand(const json &input, const std::string &expected_run_id) {
  const std::set<std::string> allowed = {"schema",           "id",    "run_id", "command",
                                         "expires_at_sim_s", "value", "fault"};
  if (!input.is_object())
    throw std::runtime_error("Comando JSON non valido.");
  for (const auto &[key, value] : input.items())
    if (!allowed.contains(key))
      throw std::runtime_error("Campo comando non riconosciuto.");
  if (input.at("schema") != "slx.reelsync.command.v1" || input.at("run_id") != expected_run_id)
    throw std::runtime_error("Il comando appartiene a una sessione diversa.");
  const auto id = input.at("id").get<std::string>();
  auto c = abi<SLX_ReelSync_Command>();
  const auto parsed = std::from_chars(id.data(), id.data() + id.size(), c.id);
  if (id.empty() || id.size() > 20 || id[0] == '0' || parsed.ec != std::errc() ||
      parsed.ptr != id.data() + id.size() || c.id == UINT64_MAX)
    throw std::runtime_error("Identificativo comando non valido.");
  static const std::map<std::string, uint32_t> types = {{"start", 1},
                                                        {"stop", 2},
                                                        {"pause", 3},
                                                        {"resume", 4},
                                                        {"reset", 5},
                                                        {"set_speed", 6},
                                                        {"set_tension", 7},
                                                        {"enter_maintenance", 8},
                                                        {"exit_maintenance", 9},
                                                        {"inject_fault", 10},
                                                        {"single_step", 12},
                                                        {"remove_fault", 13},
                                                        {"acknowledge", 14}};
  const auto type = types.find(input.at("command").get<std::string>());
  if (type == types.end())
    throw std::runtime_error("Comando non supportato.");
  c.type = type->second;
  if (!input.at("expires_at_sim_s").is_number())
    throw std::runtime_error("Scadenza comando non valida.");
  c.expires_at_sim_s = input.at("expires_at_sim_s");
  if (!std::isfinite(c.expires_at_sim_s) || c.expires_at_sim_s < 0 || c.expires_at_sim_s > 3600)
    throw std::runtime_error("Scadenza comando fuori campo.");
  const bool value_command =
      c.type == SLX_RS_SET_SPEED || c.type == SLX_RS_SET_TENSION || c.type == SLX_RS_INJECT_FAULT;
  if (value_command != input.contains("value") ||
      input.contains("fault") != (c.type == SLX_RS_INJECT_FAULT))
    throw std::runtime_error("Parametri del comando non validi.");
  if (value_command) {
    if (!input.at("value").is_number())
      throw std::runtime_error("Valore comando non numerico.");
    c.value = input.at("value");
    if (!std::isfinite(c.value))
      throw std::runtime_error("Valore comando non finito.");
  }
  if (c.type == SLX_RS_SET_SPEED && (c.value < 0 || c.value > 4))
    throw std::runtime_error("Velocita fuori campo.");
  if (c.type == SLX_RS_SET_TENSION && (c.value < 20 || c.value > 200))
    throw std::runtime_error("Tensione fuori campo.");
  if (c.type == SLX_RS_INJECT_FAULT) {
    if (!input.at("fault").is_number_integer())
      throw std::runtime_error("Causa non valida.");
    const auto fault = input.at("fault").get<int64_t>();
    if (fault < 1 || fault > 9)
      throw std::runtime_error("Causa fuori campo.");
    c.argument = uint32_t(fault);
    const auto &spec = faultCatalog[c.argument - 1];
    if (c.value < spec.min || c.value > spec.max)
      throw std::runtime_error("Intensita anomalia fuori campo.");
    if (c.argument == SLX_RS_SENSOR_LAG &&
        std::abs(c.value / .005 - std::round(c.value / .005)) > 1e-8)
      throw std::runtime_error("Il ritardo deve essere multiplo di 5 ms.");
  }
  return c;
}
} // namespace slx::reelsync
