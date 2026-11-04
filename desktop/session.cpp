// SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#include "session.hpp"
#include "alarms.hpp"
#include "educational_controller.h"
#include "faults.hpp"
#include "recipes.hpp"
#include "visual_frame.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
namespace slx::reelsync {
std::string newRunId() {
  std::random_device random;
  std::ostringstream text;
  text << "rs-" << std::hex << std::setfill('0');
  for (int i = 0; i < 4; ++i)
    text << std::setw(8) << uint32_t(random());
  return text.str();
}
void check(int32_t r) {
  static const char *messages[] = {"",
                                   "Valore o configurazione fuori dai limiti consentiti.",
                                   "Versioni dell'applicazione e del core non compatibili.",
                                   "Operazione non disponibile nello stato corrente della linea.",
                                   "Licenza offline assente o non valida.",
                                   "Funzione non disponibile con questa licenza.",
                                   "Errore di calcolo. Salva la prova e avvia una nuova sessione.",
                                   "Comando scaduto o gia superato.",
                                   "Spazio insufficiente per i dati richiesti."};
  if (r)
    throw std::runtime_error(r > 0 && r < 9 ? messages[r] : "Operazione non riuscita.");
}
const char *stateName(uint32_t s) {
  static const char *names[] = {"Configurazione", "Pronto",        "Marcia", "Arresto controllato",
                                "Fermo",          "Pausa analisi", "Guasto", "Manutenzione"};
  return s < 8 ? names[s] : "Stato sconosciuto";
}
Session::Session(const std::filesystem::path &license) : license_file(license) {
  std::ifstream f(license, std::ios::binary);
  if (!f)
    throw std::runtime_error("Licenza offline mancante: evaluation.license");
  std::string text((std::istreambuf_iterator<char>(f)), {});
  check(SLX_ReelSync_Create(SLX_RS_ABI, text.data(), uint32_t(text.size()), &h));
  build = abi<SLX_ReelSync_BuildInfo>();
  check(SLX_ReelSync_GetBuildInfo(&build));
  if (std::string(build.product) != "SLX ReelSync" ||
      std::string(build.core_version) != SLX_RS_VERSION || build.abi_version != SLX_RS_ABI) {
    SLX_ReelSync_Destroy(h);
    h = nullptr;
    throw std::runtime_error("Core SiliconLogiX incompatibile con la versione dell'host");
  }
  config = abi<SLX_ReelSync_Config>();
  check(SLX_ReelSync_DefaultConfig(&config));
  configure(config);
}
Session::~Session() {
  SLX_ReelSync_Destroy(h);
}
void Session::configure(const SLX_ReelSync_Config &c) {
  check(SLX_ReelSync_Configure(h, &c));
  config = c;
  run_id = newRunId();
  check(SLX_ReelSync_SetController(h, SLX_ReelSync_EducationalControl, nullptr));
  history.clear();
  diagnostic_history.clear();
  events.clear();
  core_event_cursor = event_sequence_offset = 0;
  plans.clear();
  length_target_m = 0;
  length_stop_armed = alarm_pause_armed = pause_on_alarm = false;
  analysis_trigger_alarms = 0;
  next_sample = 0;
  collect();
}
int32_t Session::submit(const SLX_ReelSync_Command &c) {
  auto r = SLX_ReelSync_SubmitCommand(h, &c);
  command_id = std::max(command_id, c.id);
  collect();
  if (!r && (c.type == SLX_RS_RESET || c.type == SLX_RS_CLEAR_FAULT)) {
    run_id = newRunId();
    history.clear();
    diagnostic_history.clear();
    events.clear();
    core_event_cursor = event_sequence_offset = 0;
    plans.clear();
    length_target_m = 0;
    length_stop_armed = alarm_pause_armed = pause_on_alarm = false;
    analysis_trigger_alarms = 0;
    next_sample = 0;
    collect();
  }
  return r;
}
int32_t Session::command(uint32_t type, double value, uint32_t argument) {
  auto c = abi<SLX_ReelSync_Command>();
  c.id = ++command_id;
  c.type = type;
  c.value = value;
  c.argument = argument;
  auto r = submit(c);

  return r;
}
SLX_ReelSync_Snapshot Session::snapshot() const {
  auto s = abi<SLX_ReelSync_Snapshot>();
  check(SLX_ReelSync_GetSnapshot(h, &s));
  return s;
}
SLX_ReelSync_Diagnostics Session::diagnostics() const {
  auto d = abi<SLX_ReelSync_Diagnostics>();
  check(SLX_ReelSync_GetDiagnostics(h, &d));
  return d;
}
SLX_ReelSync_WebTracking Session::webTracking() const {
  auto tracking = abi<SLX_ReelSync_WebTracking>();
  check(SLX_ReelSync_GetWebTracking(h, &tracking));
  return tracking;
}
SLX_ReelSync_MechanicalReadings Session::mechanicalReadings() const {
  auto m = abi<SLX_ReelSync_MechanicalReadings>();
  check(SLX_ReelSync_GetMechanicalReadings(h, &m));
  return m;
}
json Session::telemetry() const {
  const auto s = snapshot();
  const auto d = diagnostics();
  const auto m = mechanicalReadings();
  return {
      {"schema", "slx.reelsync.telemetry.v1"},
      {"product", "SLX ReelSync"},
      {"host_version", SLX_RS_VERSION},
      {"model", build.model_version},
      {"run_id", run_id},
      {"sequence", s.sequence},
      {"sim_s", s.sim_s},
      {"state",
       {{"code", s.state},
        {"label", (d.active_alarms & SLX_RS_ALARM_MODEL_BOUNDARY) ? "Limite modello / prova sospesa"
                                                                  : stateName(s.state)},
        {"previous", d.paused_state},
        {"model_halted", bool(d.active_alarms & SLX_RS_ALARM_MODEL_BOUNDARY)}}},
      {"connection",
       {{"source", "local-simulation"}, {"hardware_connected", false}, {"remote_control", false}}},
      {"process",
       {{"speed_m_s", s.speed_m_s},
        {"speed_setpoint_m_s", s.speed_setpoint_m_s},
        {"tension_u_n", s.tension_u_n},
        {"tension_r_n", s.tension_r_n},
        {"tension_setpoint_n", s.tension_setpoint_n},
        {"unwind_diameter_m", s.unwind_radius_m * 2},
        {"rewind_diameter_m", s.rewind_radius_m * 2},
        {"dancer_position_m", s.dancer_z_m},
        {"nip_travel_m", s.transported_m},
        {"pneumatic_force_n", m.pneumatic_force_n},
        {"pneumatic_available_fraction", m.pneumatic_available_fraction},
        {"unwind_power_w", m.unwind_power_w},
        {"rewind_power_w", m.rewind_power_w},
        {"nip_power_w", m.nip_power_w}}},
      {"sensors",
       {{"encoder_m_s", s.measured_speed_m_s},
        {"tension_n", s.measured_tension_n},
        {"quality", s.sensor_quality}}},
      {"alarms", {{"active", d.active_alarms}, {"latched", d.latched_alarms}, {"fault", s.fault}}},
      {"visual", [&] {
         std::vector<SLX_ReelSync_PathPoint> points(SLX_RS_MAX_PATH_POINTS);
         uint32_t count = 0;
         check(SLX_ReelSync_GetPath(h, points.data(), uint32_t(points.size()), &count));
         points.resize(count);
         return visualFrame(s, webTracking(), points);
       }()}};
}
void Session::setLengthTarget(double target_m) {
  if (!std::isfinite(target_m) || target_m < 0 || target_m > 10000)
    throw std::runtime_error("Lunghezza obiettivo ammessa: 0 (disattiva) oppure fino a 10000 m");
  length_target_m = target_m;
  length_stop_armed = target_m > 0;
}
json Session::eventFrame(uint64_t after) const {
  json list = json::array();
  for (const auto &e : events)
    if (e.sequence > after)
      list.push_back({{"sequence", std::to_string(e.sequence)},
                      {"command_id", std::to_string(e.command_id)},
                      {"sim_s", e.sim_s},
                      {"type", e.type},
                      {"result", e.result},
                      {"argument", e.argument},
                      {"value", e.value},
                      {"message", e.message}});
  const uint64_t first = events.empty() ? 0 : events.front().sequence,
                 last = events.empty() ? 0 : events.back().sequence;
  return {{"schema", "slx.reelsync.events.v1"},
          {"run_id", run_id},
          {"after_sequence", std::to_string(after)},
          {"first_sequence", std::to_string(first)},
          {"last_sequence", std::to_string(last)},
          {"resync_required", after > last || (after > 0 && first > 0 && after < first - 1)},
          {"events", list}};
}
void Session::setPauseOnAlarm(bool enabled) {
  pause_on_alarm = alarm_pause_armed = enabled;
  analysis_trigger_alarms = 0;
}
void Session::schedule(FaultPlan p) {
  auto s = snapshot();
  if (plans.size() >= 32 || p.fault < 1 || p.fault > 9 || !std::isfinite(p.at_s) ||
      !std::isfinite(p.duration_s) || !std::isfinite(p.value) || p.at_s < s.sim_s ||
      p.at_s > 3600 || p.duration_s < 0 || p.duration_s > 600)
    throw std::runtime_error("Piano guasti fuori campo");
  const auto &spec = faultCatalog[p.fault - 1];
  if (p.fault == SLX_RS_SENSOR_LAG && std::abs(p.value / .005 - std::round(p.value / .005)) > 1e-8)
    throw std::runtime_error("Il ritardo sensori richiede multipli di 5 ms");
  if ((spec.max > spec.min && (p.value < spec.min || p.value > spec.max)) ||
      (!spec.reversible && p.duration_s > 0))
    throw std::runtime_error("Intensita o ripristino non ammessi per questa anomalia");
  plans.push_back(p);
}
json Session::alarmStatus() const {
  const auto s = snapshot();
  const auto d = diagnostics();
  json items = json::array(), pending = json::array();
  const bool can_ack =
      d.latched_alarms && !s.fault && (d.command_flags & SLX_RS_DIAG_CAN_ACKNOWLEDGE);
  const std::string recovery =
      s.fault == SLX_RS_WEB_BREAK ? "Materiale rotto: preparare una nuova prova"
      : s.fault                   ? "Causa di prova attiva: rimuoverla dal simulatore"
      : d.active_alarms           ? "Allarmi attivi: verificare le condizioni della linea"
      : can_ack                   ? "Condizioni rientrate: confermare il ripristino"
      : d.latched_alarms          ? "Attendere arresto completo e fine della pausa analisi"
                                  : "Nessun allarme in memoria";
  std::array<double, 9> since{};
  uint32_t previous = 0;
  for (const auto &e : events)
    if (e.type == 105) {
      const auto mask = uint32_t(e.result);
      for (unsigned i = 0; i < 9; ++i)
        if ((mask & (1u << i)) && !(previous & (1u << i)))
          since[i] = e.sim_s;
      previous = mask;
    }
  for (unsigned i = 0; i < 9; ++i) {
    const uint32_t bit = 1u << i;
    if (!((d.active_alarms | d.latched_alarms) & bit))
      continue;
    items.push_back({{"code", alarmCatalog[i].code},
                     {"title", alarmNames[i]},
                     {"component", alarmCatalog[i].component},
                     {"action", alarmCatalog[i].action},
                     {"active", bool(d.active_alarms & bit)},
                     {"latched", bool(d.latched_alarms & bit)},
                     {"since_s", since[i]}});
  }
  for (size_t i = 0; i < plans.size(); ++i)
    if (!plans[i].injected)
      pending.push_back({{"index", i},
                         {"name", faultName(plans[i].fault)},
                         {"fault", plans[i].fault},
                         {"at_s", plans[i].at_s},
                         {"remaining_s", std::max(0., plans[i].at_s - s.sim_s)}});
  std::stable_sort(items.begin(), items.end(), [](const json &a, const json &b) {
    return a["active"].get<bool>() > b["active"].get<bool>();
  });
  std::sort(pending.begin(), pending.end(), [](const json &a, const json &b) {
    return a["at_s"].get<double>() < b["at_s"].get<double>();
  });
  return {{"active", d.active_alarms},
          {"latched", d.latched_alarms},
          {"active_count", alarmCount(d.active_alarms)},
          {"memory_count", alarmCount(d.latched_alarms & ~d.active_alarms)},
          {"items", items},
          {"test_cause",
           {{"id", s.fault},
            {"name", faultName(s.fault)},
            {"description", s.fault ? faultCatalog[s.fault - 1].description : ""},
            {"value", d.fault_severity}}},
          {"attention_parts", attentionParts(s.fault, d.active_alarms)},
          {"pending", pending},
          {"can_acknowledge", can_ack},
          {"recovery_hint", recovery},
          {"length_target_m", length_target_m},
          {"length_stop_armed", length_stop_armed}};
}
std::vector<SLX_ReelSync_PathPoint> Session::path() {
  std::vector<SLX_ReelSync_PathPoint> p(SLX_RS_MAX_PATH_POINTS);
  uint32_t n = 0;
  check(SLX_ReelSync_GetPath(h, p.data(), uint32_t(p.size()), &n));
  p.resize(n);
  return p;
}
void Session::step(uint32_t n) {
  for (uint32_t i = 0; i < n; ++i) {
    if (diagnostics().active_alarms & SLX_RS_ALARM_MODEL_BOUNDARY)
      break;
    auto before = snapshot();
    if (length_stop_armed && before.state == SLX_RS_RUNNING) {
      const double speed = std::max(0., before.speed_m_s);
      const double braking = speed * speed / (2 * config.acceleration_m_s2);
      if (length_target_m - before.transported_m <= braking + speed * config.dt_s) {
        check(command(SLX_RS_STOP));
        length_stop_armed = false;
      }
    }
    for (auto &p : plans) {
      if (!p.injected && before.sim_s + 1e-10 >= p.at_s && before.state != SLX_RS_PAUSED &&
          before.state != SLX_RS_READY) {
        p.injection_result = command(SLX_RS_INJECT_FAULT, p.value, p.fault);
        p.injected = true;
      }
      if (p.injected && !p.injection_result && !p.removed && p.duration_s > 0 &&
          before.sim_s + 1e-10 >= p.at_s + p.duration_s) {
        p.removal_result = command(SLX_RS_REMOVE_FAULT);
        p.removed = true;
      }
    }
    check(SLX_ReelSync_Step(h));
    auto s = snapshot();
    const auto current_diagnostics = diagnostics();
    const bool halted = (current_diagnostics.active_alarms & SLX_RS_ALARM_MODEL_BOUNDARY) != 0;
    if (s.sequence >= next_sample ||
        (halted && (history.empty() || history.back().sequence != s.sequence))) {
      if (history.size() >= 180001)
        throw std::runtime_error(
            "Limite registrazione: esportare e ripristinare la prova (3600 s)");
      history.push_back(s);
      diagnostic_history.push_back(diagnostics());
      next_sample = s.sequence + uint64_t(std::llround(.02 / config.dt_s));
      collect();
    }
    if (halted) {
      if (!history.empty() && history.back().sequence == s.sequence) {
        history.back() = s;
        diagnostic_history.back() = current_diagnostics;
      }
      break;
    }
    if (alarm_pause_armed &&
        (s.state == SLX_RS_RUNNING || s.state == SLX_RS_STOPPING || s.state == SLX_RS_FAULT)) {
      auto d = diagnostics();
      if (d.active_alarms && !command(SLX_RS_PAUSE)) {
        analysis_trigger_alarms = d.active_alarms;
        alarm_pause_armed = false;
      }
    }
  }
  collect();
}
void Session::collect() {
  auto s = snapshot();
  if (history.empty()) {
    history.push_back(s);
    diagnostic_history.push_back(diagnostics());
    next_sample = s.sequence + uint64_t(std::llround(.02 / config.dt_s));
  }
  SLX_ReelSync_Event buffer[256];
  uint32_t n = 0;
  check(SLX_ReelSync_ReadEvents(h, core_event_cursor, buffer, 256, &n));
  for (uint32_t i = 0; i < n; ++i) {
    core_event_cursor = buffer[i].sequence;
    buffer[i].sequence += event_sequence_offset;
  }
  events.insert(events.end(), buffer, buffer + n);
}
json configurationOf(const SLX_ReelSync_Config &config) {
  return {{"dt_s", config.dt_s},
          {"width_m", config.width_m},
          {"thickness_m", config.thickness_m},
          {"density_kg_m3", config.density_kg_m3},
          {"young_pa", config.young_pa},
          {"unwind_radius_m", config.unwind_radius_m},
          {"rewind_radius_m", config.rewind_radius_m},
          {"tension_n", config.tension_n},
          {"speed_m_s", config.speed_m_s},
          {"acceleration_m_s2", config.acceleration_m_s2},
          {"torque_limit_nm", config.torque_limit_nm},
          {"dancer_mass_kg", config.dancer_mass_kg},
          {"sensor_noise_n", config.sensor_noise_n},
          {"sensor_bias_n", config.sensor_bias_n},
          {"sensor_delay_s", config.sensor_delay_s},
          {"seed", config.seed},
          {"controller", config.controller == 1 ? "reference-1" : "educational-p-1"}};
}
json Session::configuration() const {
  return configurationOf(config);
}
SLX_ReelSync_Config configurationFrom(const json &input, SLX_ReelSync_Config c) {
  const json &r = input.contains("configuration") ? input.at("configuration") : input;
#define SLX_FIELD(f)                                                                               \
  if (r.contains(#f))                                                                              \
  c.f = r.at(#f).get<decltype(c.f)>()
  SLX_FIELD(dt_s);
  SLX_FIELD(width_m);
  SLX_FIELD(thickness_m);
  SLX_FIELD(density_kg_m3);
  SLX_FIELD(young_pa);
  SLX_FIELD(unwind_radius_m);
  SLX_FIELD(rewind_radius_m);
  SLX_FIELD(tension_n);
  SLX_FIELD(speed_m_s);
  SLX_FIELD(acceleration_m_s2);
  SLX_FIELD(torque_limit_nm);
  SLX_FIELD(dancer_mass_kg);
  SLX_FIELD(sensor_noise_n);
  SLX_FIELD(sensor_bias_n);
  SLX_FIELD(sensor_delay_s);
#undef SLX_FIELD
  if (r.contains("seed")) {
    if (!r["seed"].is_number_unsigned() &&
        (!r["seed"].is_number_integer() || r["seed"].get<int64_t>() < 0))
      throw std::runtime_error("Seed non valido");
    c.seed = r.at("seed").get<uint64_t>();
  }
  if (r.contains("controller")) {
    std::string controller = r.at("controller");
    if (controller == "reference-1")
      c.controller = 1;
    else if (controller == "educational-p-1")
      c.controller = 2;
    else
      throw std::runtime_error("Controllore della ricetta non disponibile");
  }
  return c;
}
json Session::metrics() const {
  if (history.empty())
    return json::object();
  double sq = 0, mx = 0, over = 0, zmin = 1e9, zmax = -1e9, balance = 0;
  size_t saturated = 0;
  for (const auto &s : history) {
    double e = s.tension_r_n - s.tension_setpoint_n;
    sq += e * e;
    mx = std::max(mx, std::abs(e));
    over = std::max(over, e);
    zmin = std::min(zmin, s.dancer_z_m);
    zmax = std::max(zmax, s.dancer_z_m);
    balance = std::max(balance, std::abs(s.material_balance_error_m));
    if (s.saturated)
      ++saturated;
  }
  // Settling only for final setpoint segment, within +/-2% continuously through end for >=1s.
  size_t start = 0;
  for (size_t i = 1; i < history.size(); ++i)
    if (history[i].tension_setpoint_n != history[i - 1].tension_setpoint_n)
      start = i;
  size_t last_bad = start;
  bool bad = false;
  for (size_t i = start; i < history.size(); ++i)
    if (std::abs(history[i].tension_r_n - history[i].tension_setpoint_n) >
        .02 * history[i].tension_setpoint_n) {
      last_bad = i;
      bad = true;
    }
  size_t candidate = bad ? last_bad + 1 : start;
  json settling = nullptr;
  if (candidate < history.size() && history.back().sim_s - history[candidate].sim_s >= 1)
    settling = history[candidate].sim_s - history[start].sim_s;
  return {{"samples", history.size()},
          {"sampling_interval_s", .02},
          {"tension_rms_error_n", std::sqrt(sq / history.size())},
          {"tension_max_abs_error_n", mx},
          {"tension_overshoot_n", over},
          {"settling_final_setpoint_s", settling},
          {"saturated_sample_fraction", double(saturated) / history.size()},
          {"dancer_min_z_m", zmin},
          {"dancer_max_z_m", zmax},
          {"max_abs_material_balance_error_m", balance}};
}
json Session::recording(const std::string &scenario) const {
  json log = json::array();
  for (const auto &e : events)
    log.push_back({{"sequence", e.sequence},
                   {"command_id", e.command_id},
                   {"sim_s", e.sim_s},
                   {"type", e.type},
                   {"argument", e.argument},
                   {"value", e.value},
                   {"result", e.result},
                   {"message", e.message}});
  json report = {{"schema", "slx.reelsync.trial.v1"},
                 {"product", "SLX ReelSync"},
                 {"scenario", scenario},
                 {"host_version", SLX_RS_VERSION},
                 {"core_version", build.core_version},
                 {"sdk_abi", SLX_RS_ABI},
                 {"model_version", build.model_version},
                 {"build_id", build.build_id},
                 {"configuration", configuration()},
                 {"material", materialInfo(config)},
                 {"events", log},
                 {"metrics", metrics()},
                 {"limits", "Reduced elastic two-zone model; engineering examples; no real-line "
                            "calibration or certified performance."}};
  report["controller_source_sha256"] =
      config.controller == 2 ? json(SLX_ReelSync_EducationalSourceHash()) : json(nullptr);
  json schedule = json::array();
  for (const auto &p : plans)
    schedule.push_back({{"fault", p.fault},
                        {"at_s", p.at_s},
                        {"duration_s", p.duration_s},
                        {"value", p.value},
                        {"injected", p.injected},
                        {"removed", p.removed},
                        {"injection_result", p.injection_result},
                        {"removal_result", p.removal_result}});
  report["fault_schedule"] = schedule;
  report["operator_tools"] = {{"length_target_m", length_target_m},
                              {"pause_on_alarm", pause_on_alarm},
                              {"analysis_trigger_alarms", analysis_trigger_alarms}};
  auto d = abi<SLX_ReelSync_Diagnostics>();
  check(SLX_ReelSync_GetDiagnostics(h, &d));
  report["final_diagnostics"] = {{"active_alarms", d.active_alarms},
                                 {"latched_alarms", d.latched_alarms},
                                 {"fault_severity", d.fault_severity}};
  auto final = abi<SLX_ReelSync_Snapshot>();
  check(SLX_ReelSync_GetSnapshot(h, &final));
  report["final_time_s"] = final.sim_s;
  report["run_id"] = run_id;
  report["host_state"] = {{"length_stop_armed", length_stop_armed},
                          {"alarm_pause_armed", alarm_pause_armed},
                          {"command_id", command_id}};
  report["operator_tools"]["final_length_error_m"] =
      length_target_m > 0 ? json(final.transported_m - length_target_m) : json(nullptr);
  return report;
}
void Session::restore(const json &record) {
  if (record.at("schema") != "slx.reelsync.trial.v1" || record.at("build_id") != build.build_id ||
      record.at("model_version") != build.model_version)
    throw std::runtime_error(
        "Questa sessione richiede la versione del core con cui e stata salvata.");
  if (!record.at("controller_source_sha256").is_null() &&
      record.at("controller_source_sha256") != SLX_ReelSync_EducationalSourceHash())
    throw std::runtime_error(
        "Il controllore didattico e diverso da quello della sessione salvata.");
  const double final_time = record.at("final_time_s");
  if (!std::isfinite(final_time) || final_time < 0 || final_time > 3600 ||
      !record.at("events").is_array() || record.at("events").empty() ||
      record.at("events").size() > 100000)
    throw std::runtime_error("Registrazione di sessione non valida.");
  Session replacement(license_file);
  replacement.configure(configurationFrom(record, replacement.config));
  replacement.run_id = record.at("run_id").get<std::string>();
  if (replacement.run_id.size() != 35 || !replacement.run_id.starts_with("rs-") ||
      !std::all_of(replacement.run_id.begin() + 3, replacement.run_id.end(),
                   [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
    throw std::runtime_error("Identificativo della sessione non valido.");
  uint64_t iterations = 0, last_sequence = 0, largest_command = 0;
  double last_time = 0;
  std::vector<SLX_ReelSync_Event> saved_events;
  auto advanceTo = [&](double time) {
    if (!std::isfinite(time) || time < last_time || time > final_time + 1e-8)
      throw std::runtime_error("Sequenza temporale non valida nella sessione.");
    while (replacement.snapshot().sim_s < time - replacement.config.dt_s * .25) {
      const auto before = replacement.snapshot().sim_s;
      replacement.step();
      if (++iterations > 30000000 || replacement.snapshot().sim_s == before)
        throw std::runtime_error("Impossibile ricostruire la sessione da questo stato.");
    }
    last_time = time;
  };
  for (const auto &e : record.at("events")) {
    auto saved = abi<SLX_ReelSync_Event>();
    saved.sequence = e.at("sequence");
    saved.command_id = e.at("command_id");
    saved.sim_s = e.at("sim_s");
    saved.type = e.at("type");
    saved.argument = e.at("argument");
    saved.value = e.at("value");
    saved.result = e.at("result");
    const auto message = e.at("message").get<std::string>();
    if (saved.sequence <= last_sequence || saved.sequence > uint64_t(INT64_MAX) ||
        saved.command_id == UINT64_MAX || !std::isfinite(saved.value) ||
        message.size() >= sizeof(saved.message))
      throw std::runtime_error("Evento non valido nella sessione salvata.");
    advanceTo(saved.sim_s);
    last_sequence = saved.sequence;
    largest_command = std::max(largest_command, saved.command_id);
    std::memcpy(saved.message, message.c_str(), message.size() + 1);
    saved_events.push_back(saved);
    if (saved.type == 106 && replacement.snapshot().state != SLX_RS_PAUSED &&
        !(replacement.diagnostics().active_alarms & SLX_RS_ALARM_MODEL_BOUNDARY)) {
      replacement.step();
      if (!(replacement.diagnostics().active_alarms & SLX_RS_ALARM_MODEL_BOUNDARY) ||
          replacement.snapshot().sim_s != saved.sim_s)
        throw std::runtime_error("Limite del modello non riproducibile nella sessione.");
    }
    if (saved.type >= 1 && saved.type <= 14 && saved.result == 0) {
      auto command = abi<SLX_ReelSync_Command>();
      command.id = saved.command_id;
      command.type = saved.type;
      command.argument = saved.argument;
      command.value = saved.value;
      check(replacement.submit(command));
    }
  }
  advanceTo(final_time);
  replacement.command_id = largest_command;
  replacement.event_sequence_offset =
      int64_t(last_sequence) - int64_t(replacement.core_event_cursor);
  replacement.events = std::move(saved_events);
  const auto &tools = record.at("operator_tools");
  replacement.setLengthTarget(tools.at("length_target_m"));
  replacement.setPauseOnAlarm(tools.at("pause_on_alarm"));
  replacement.analysis_trigger_alarms = tools.at("analysis_trigger_alarms");
  if (record.contains("host_state")) {
    replacement.length_stop_armed = record.at("host_state").at("length_stop_armed");
    replacement.alarm_pause_armed = record.at("host_state").at("alarm_pause_armed");
    const auto saved_command = record.at("host_state").value("command_id", largest_command);
    if (saved_command == UINT64_MAX)
      throw std::runtime_error("Identificativo comando non valido.");
    replacement.command_id = std::max(largest_command, saved_command);
  } else {
    replacement.length_stop_armed = false;
    replacement.alarm_pause_armed = false;
  }
  if (record.at("fault_schedule").size() > 32)
    throw std::runtime_error("Troppi eventi programmati.");
  for (const auto &p : record.at("fault_schedule")) {
    FaultPlan plan{p.at("fault"), p.at("at_s"), p.at("duration_s"), p.at("value")};
    if (plan.fault < 1 || plan.fault > faultCatalog.size() || !std::isfinite(plan.at_s) ||
        !std::isfinite(plan.duration_s) || !std::isfinite(plan.value) || plan.at_s < 0 ||
        plan.at_s > 3600 || plan.duration_s < 0 || plan.duration_s > 600)
      throw std::runtime_error("Evento programmato non valido.");
    const auto &spec = faultCatalog[plan.fault - 1];
    if ((spec.max > spec.min && (plan.value < spec.min || plan.value > spec.max)) ||
        (!spec.reversible && plan.duration_s > 0))
      throw std::runtime_error("Parametri dell'anomalia fuori campo.");
    plan.injected = p.at("injected");
    plan.removed = p.at("removed");
    plan.injection_result = p.at("injection_result");
    plan.removal_result = p.at("removal_result");
    replacement.plans.push_back(plan);
  }
  replacement.run_id = record.at("run_id").get<std::string>();
  using std::swap;
  swap(h, replacement.h);
  swap(command_id, replacement.command_id);
  swap(next_sample, replacement.next_sample);
  swap(run_id, replacement.run_id);
  swap(core_event_cursor, replacement.core_event_cursor);
  swap(event_sequence_offset, replacement.event_sequence_offset);
  swap(config, replacement.config);
  swap(build, replacement.build);
  swap(history, replacement.history);
  swap(events, replacement.events);
  swap(diagnostic_history, replacement.diagnostic_history);
  swap(plans, replacement.plans);
  swap(length_target_m, replacement.length_target_m);
  swap(pause_on_alarm, replacement.pause_on_alarm);
  swap(analysis_trigger_alarms, replacement.analysis_trigger_alarms);
  swap(length_stop_armed, replacement.length_stop_armed);
  swap(alarm_pause_armed, replacement.alarm_pause_armed);
}
void Session::exportTrial(const std::filesystem::path &dir, const std::string &scenario) const {
  std::filesystem::create_directories(dir);
  std::ofstream csv(dir / "samples.csv");
  if (!csv)
    throw std::runtime_error("Impossibile scrivere CSV");
  csv << std::setprecision(17)
      << "sim_s,state,speed_m_s,speed_ref_m_s,tension_u_n,tension_r_n,tension_ref_n,measured_"
         "tension_n,unwind_diameter_m,rewind_diameter_m,unwind_torque_nm,rewind_torque_nm,nip_"
         "torque_nm,dancer_z_m,balance_error_m,fault,sensor_quality,saturated,active_alarms,"
         "latched_alarms,encoder_error_m_s,torque_utilization\n";
  for (size_t i = 0; i < history.size(); ++i) {
    const auto &s = history[i];
    const auto &d = diagnostic_history[i];
    csv << s.sim_s << ',' << s.state << ',' << s.speed_m_s << ',' << s.speed_setpoint_m_s << ','
        << s.tension_u_n << ',' << s.tension_r_n << ',' << s.tension_setpoint_n << ','
        << s.measured_tension_n << ',' << 2 * s.unwind_radius_m << ',' << 2 * s.rewind_radius_m
        << ',' << s.unwind_torque_nm << ',' << s.rewind_torque_nm << ',' << s.nip_torque_nm << ','
        << s.dancer_z_m << ',' << s.material_balance_error_m << ',' << s.fault << ','
        << s.sensor_quality << ',' << s.saturated << ',' << d.active_alarms << ','
        << d.latched_alarms << ',' << d.encoder_error_m_s << ',' << d.torque_utilization << '\n';
  }
  json report = recording(scenario);
  auto final = abi<SLX_ReelSync_Snapshot>();
  check(SLX_ReelSync_GetSnapshot(h, &final));
  std::ofstream jsonfile(dir / "trial.json");
  jsonfile << report.dump(2) << '\n';
  std::ofstream telemetry_file(dir / "telemetry.json");
  telemetry_file << telemetry().dump(2) << '\n';
  std::ofstream event_file(dir / "events.json");
  event_file << eventFrame().dump(2) << '\n';
  std::ofstream md(dir / "report.md");
  md << "# SLX ReelSync — report della prova\n\nScenario: " << scenario
     << ". Modello: " << build.model_version << ". Core/host/SDK: " << SLX_RS_VERSION
     << " / ABI 1.\n\n## Metriche\n\n```json\n"
     << metrics().dump(2)
     << "\n```\n\nConfigurazione, seed ed eventi con esiti sono in trial.json. Campioni "
        "sincronizzati a 50 Hz in samples.csv.\n\nAssestamento: ingresso nella banda +/-2% del "
        "riferimento finale, mantenuta fino al termine per almeno 1 s; null significa non "
        "definito/osservazione insufficiente. Saturazioni: frazione di campioni a 50 Hz, non "
        "durata esatta degli impulsi.\n\nModello ridotto non calibrato su misure reali; ricette "
        "ingegneristiche illustrative. Nessuna prestazione produttiva o certificazione "
        "dichiarata.\n";
  if (length_target_m > 0)
    md << "\n## Arresto a metratura\n\nObiettivo al nip: " << length_target_m
       << " m. Materiale passato al nip: " << final.transported_m
       << " m. Scarto al momento dell'esportazione: " << final.transported_m - length_target_m
       << " m. Stato: " << stateName(final.state)
       << ". Anticipo nominale sulla rampa; nessuna accuratezza garantita sotto guasto o "
          "saturazione.\n";
  if (pause_on_alarm)
    md << "\nTrigger di pausa analisi abilitato. Maschera allarmi acquisita: "
       << analysis_trigger_alarms << ". La pausa congela il tempo e non simula una frenata.\n";
  if (!csv || !jsonfile || !md || !telemetry_file || !event_file)
    throw std::runtime_error("Esportazione incompleta");
}
void runScenario(Session &session, const std::string &scenario, double seconds,
                 const std::atomic<bool> *cancellation) {
  auto spec = std::find_if(scenarioCatalog.begin(), scenarioCatalog.end(),
                           [&](const auto &p) { return p.id == scenario; });
  if (spec == scenarioCatalog.end())
    throw std::runtime_error("Scenario sconosciuto");
  if (spec->fault)
    session.schedule({spec->fault, 20, spec->recover ? 10. : 0., spec->severity});
  check(session.command(SLX_RS_START));
  bool changed = false, disturbed = false, stopped = false;
  const uint64_t steps = uint64_t(std::llround(seconds / session.config.dt_s));
  for (uint64_t i = 0; i < steps; ++i) {
    if (session.diagnostics().active_alarms & SLX_RS_ALARM_MODEL_BOUNDARY)
      break;
    double t = i * session.config.dt_s;
    if (cancellation && i % 200 == 0 && cancellation->load())
      throw std::runtime_error("Campagna interrotta");
    if (!changed && t >= 10 && scenario == "setpoint") {
      check(session.command(SLX_RS_SET_SPEED, 3));
      check(session.command(SLX_RS_SET_TENSION, 100));
      changed = true;
    }
    if (!disturbed && t >= 24 &&
        (scenario == "drive" || scenario == "stuck" || scenario == "lag")) {
      check(session.command(scenario == "drive" ? SLX_RS_SET_SPEED : SLX_RS_SET_TENSION,
                            scenario == "drive" ? 3 : 100));
      disturbed = true;
    }
    if (!stopped && t >= 45 && session.snapshot().state == SLX_RS_RUNNING) {
      check(session.command(SLX_RS_STOP));
      stopped = true;
    }
    session.step();
  }
}
std::filesystem::path applicationRoot(const char *argv0, bool require_assets) {
  auto exe = std::filesystem::absolute(argv0).parent_path();
  for (auto candidate = exe; !candidate.empty(); candidate = candidate.parent_path()) {
    if (std::filesystem::exists(candidate / "evaluation.license") &&
        (!require_assets ||
         std::filesystem::exists(candidate / "assets/scene/SLX_ReelSync_Runtime.glb")))
      return candidate;
    if (candidate == candidate.parent_path())
      break;
  }
  if (std::filesystem::exists(std::filesystem::current_path() / "evaluation.license") &&
      (!require_assets || std::filesystem::exists(std::filesystem::current_path() /
                                                  "assets/scene/SLX_ReelSync_Runtime.glb")))
    return std::filesystem::current_path();
  throw std::runtime_error(
      "Pacchetto incompleto: evaluation.license non trovata vicino all'eseguibile");
}
} // namespace slx::reelsync
