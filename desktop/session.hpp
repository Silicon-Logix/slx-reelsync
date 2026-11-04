// SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#pragma once
#include <atomic>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <slx/reelsync.h>
#include <stdexcept>
#include <string>
#include <vector>
namespace slx::reelsync {
using json = nlohmann::json;
template <class T> T abi() {
  T v{};
  v.struct_size = sizeof(T);
  v.abi_version = SLX_RS_ABI;
  return v;
}
void check(int32_t result);
const char *stateName(uint32_t);
json configurationOf(const SLX_ReelSync_Config &);
SLX_ReelSync_Config configurationFrom(const json &, SLX_ReelSync_Config);
struct FaultPlan {
  uint32_t fault;
  double at_s, duration_s, value;
  bool injected = false, removed = false;
  int32_t injection_result = 0, removal_result = 0;
};
struct Transport {
  virtual ~Transport() = default;
  virtual int32_t submit(const SLX_ReelSync_Command &) = 0;
  virtual SLX_ReelSync_Snapshot snapshot() const = 0;
};
// Local transport has no sockets. Future adapters implement the same application boundary.
class Session final : public Transport {
  SLX_ReelSync_Handle h = nullptr;
  uint64_t command_id = 0;
  uint64_t core_event_cursor = 0;
  int64_t event_sequence_offset = 0;
  std::filesystem::path license_file;
  std::string run_id;
  uint64_t next_sample = 0;
  bool length_stop_armed = false, alarm_pause_armed = false;

public:
  SLX_ReelSync_Config config{};
  SLX_ReelSync_BuildInfo build{};
  std::vector<SLX_ReelSync_Snapshot> history;
  std::vector<SLX_ReelSync_Event> events;
  std::vector<SLX_ReelSync_Diagnostics> diagnostic_history;
  std::vector<FaultPlan> plans;
  double length_target_m = 0;
  bool pause_on_alarm = false;
  uint32_t analysis_trigger_alarms = 0;
  explicit Session(const std::filesystem::path &license_file);
  ~Session();
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  void configure(const SLX_ReelSync_Config &);
  int32_t submit(const SLX_ReelSync_Command &) override;
  int32_t command(uint32_t type, double value = 0, uint32_t argument = 0);
  SLX_ReelSync_Snapshot snapshot() const override;
  SLX_ReelSync_Diagnostics diagnostics() const;
  SLX_ReelSync_WebTracking webTracking() const;
  SLX_ReelSync_MechanicalReadings mechanicalReadings() const;
  json telemetry() const;
  json eventFrame(uint64_t after_sequence = 0) const;
  json alarmStatus() const;
  json recording(const std::string &scenario) const;
  void restore(const json &recording);
  void setLengthTarget(double target_m);
  void setPauseOnAlarm(bool enabled);
  void schedule(FaultPlan);
  std::vector<SLX_ReelSync_PathPoint> path();
  void step(uint32_t count = 1);
  void collect();
  uint64_t nextCommandId() const {
    return command_id + 1;
  }
  json metrics() const;
  json configuration() const;
  void exportTrial(const std::filesystem::path &directory, const std::string &scenario) const;
};
void runScenario(Session &, const std::string &, double seconds,
                 const std::atomic<bool> *cancellation = nullptr);
std::filesystem::path applicationRoot(const char *argv0, bool require_assets = true);
} // namespace slx::reelsync
