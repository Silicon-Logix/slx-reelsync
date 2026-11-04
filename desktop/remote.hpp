// Copyright (c) 2026 Marco Pezzullo / Silicon LogiX.
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#pragma once
#include "session.hpp"
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
namespace slx::reelsync {
struct RemoteRequest {
  json wire;
  std::chrono::steady_clock::time_point expires;
};
// The worker owns local IPC. Session is accessed exclusively by its existing UI owner.
class RemoteHost {
  std::filesystem::path directory;
  mutable std::mutex mutex;
  json state, status = json::object();
  std::deque<json> actions, results;
  std::deque<RemoteRequest> requests;
  std::atomic<bool> stopping{false};
  std::thread worker;
  void *process = nullptr, *job = nullptr;
  unsigned long process_id = 0;
  std::string epoch;
  bool overridden = false;
  std::chrono::steady_clock::time_point published;
  void run();

public:
  RemoteHost(const std::filesystem::path &root, const std::filesystem::path &data, bool enabled);
  ~RemoteHost();
  void publish(json value);
  json snapshot() const;
  void action(json value);
  void takeLocal();
  std::string authorityEpoch() const;
  std::vector<RemoteRequest> drain();
  bool permitted(const RemoteRequest &) const;
  void complete(const RemoteRequest &, json result, json current_state);
};
// Called only on the owner of Session; no network or filesystem paths supplied by a peer.
json executeRemote(Session &, const json &payload, const std::filesystem::path &results_root);
json remoteState(const Session &, bool can_remote);
} // namespace slx::reelsync
