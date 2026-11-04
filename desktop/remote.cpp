// Copyright (c) 2026 Marco Pezzullo / Silicon LogiX.
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#include "remote.hpp"
#include "faults.hpp"
#include "protocol.hpp"
#include "recipes.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
namespace slx::reelsync {
namespace {
std::string nonce() {
  std::random_device r;
  std::ostringstream s;
  for (int i = 0; i < 4; ++i)
    s << std::hex << r();
  return s.str();
}
json post(const std::string &address, const std::string &token, const json &body) {
  const auto port = std::stoi(address.substr(address.find_last_of(':') + 1));
  const std::string data = body.dump();
  std::string response;
#ifdef _WIN32
  struct Guard {
    HINTERNET h = nullptr;
    ~Guard() {
      if (h)
        WinHttpCloseHandle(h);
    }
  };
  Guard session{WinHttpOpen(L"SLX-ReelSync/1.1", WINHTTP_ACCESS_TYPE_NO_PROXY,
                            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
  if (!session.h)
    throw std::runtime_error("IPC non disponibile");
  WinHttpSetTimeouts(session.h, 250, 250, 250, 350);
  Guard connection{WinHttpConnect(session.h, L"127.0.0.1", INTERNET_PORT(port), 0)};
  Guard request{WinHttpOpenRequest(connection.h, L"POST", L"/v1/tick", nullptr, WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES, 0)};
  std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer " +
                         std::wstring(token.begin(), token.end()) + L"\r\n";
  if (!request.h ||
      !WinHttpSendRequest(request.h, headers.c_str(), DWORD(headers.size()),
                          const_cast<char *>(data.data()), DWORD(data.size()), DWORD(data.size()),
                          0) ||
      !WinHttpReceiveResponse(request.h, nullptr))
    throw std::runtime_error("Servizio pannello in avvio o non disponibile");
  for (;;) {
    char b[8192];
    DWORD n = 0;
    if (!WinHttpReadData(request.h, b, sizeof(b), &n))
      throw std::runtime_error("Lettura IPC non riuscita");
    if (!n)
      break;
    response.append(b, n);
    if (response.size() > 256 * 1024)
      throw std::runtime_error("Risposta IPC troppo grande");
  }
#else
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  struct Guard {
    int fd;
    ~Guard() {
      if (fd >= 0)
        close(fd);
    }
  } guard{fd};
  timeval timeout{0, 350000};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(uint16_t(port));
  inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
  if (connect(fd, reinterpret_cast<sockaddr *>(&a), sizeof(a)))
    throw std::runtime_error("IPC non disponibile");
  const std::string wire =
      "POST /v1/tick HTTP/1.0\r\nHost: localhost\r\nAuthorization: Bearer " + token +
      "\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(data.size()) +
      "\r\nConnection: close\r\n\r\n" + data;
  size_t off = 0;
  while (off < wire.size()) {
    auto n = send(fd, wire.data() + off, wire.size() - off, MSG_NOSIGNAL);
    if (n <= 0)
      throw std::runtime_error("Invio IPC fallito");
    off += size_t(n);
  }
  for (;;) {
    char b[8192];
    auto n = recv(fd, b, sizeof(b), 0);
    if (!n)
      break;
    if (n < 0)
      throw std::runtime_error("Lettura IPC fallita");
    response.append(b, size_t(n));
    if (response.size() > 256 * 1024)
      throw std::runtime_error("IPC troppo grande");
  }
  const auto split = response.find("\r\n\r\n");
  if (split == std::string::npos)
    throw std::runtime_error("IPC incompleta");
  response.erase(0, split + 4);
#endif
  return json::parse(response);
}
} // namespace
RemoteHost::RemoteHost(const std::filesystem::path &root, const std::filesystem::path &data,
                       bool enabled)
    : directory(data / "Remote"), epoch(nonce()) {
  if (!enabled)
    return;
  std::filesystem::create_directories(directory);
#ifdef _WIN32
  const auto exe = root / "bin/slx-reelsync-link.exe";
  if (!std::filesystem::is_regular_file(exe)) {
    status = {{"error", "Servizio pannello non presente nel pacchetto"}};
    return;
  }
  std::wstring cmd =
      L"\"" + exe.wstring() + L"\" --role pc --data \"" + directory.wstring() + L"\"";
  if (GetEnvironmentVariableW(L"SLX_RS_LOOPBACK_TEST", nullptr, 0))
    cmd += L" --listen 127.0.0.1:0";
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION p{};
  if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, root.c_str(), &startup, &p)) {
    status = {{"error", "Avvio del servizio pannello non riuscito"}};
    return;
  }
  job = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!job ||
      !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
      !AssignProcessToJobObject(job, p.hProcess)) {
    TerminateProcess(p.hProcess, 1);
    CloseHandle(p.hThread);
    CloseHandle(p.hProcess);
    if (job)
      CloseHandle(job);
    job = nullptr;
    status = {{"error", "Supervisione del servizio non disponibile"}};
    return;
  }
  process = p.hProcess;
  process_id = p.dwProcessId;
  ResumeThread(p.hThread);
  CloseHandle(p.hThread);
#endif
  worker = std::thread(&RemoteHost::run, this);
}
RemoteHost::~RemoteHost() {
  stopping = true;
  if (worker.joinable())
    worker.join();
#ifdef _WIN32
  if (job)
    CloseHandle(job);
  if (process)
    CloseHandle(process);
#endif
}
void RemoteHost::publish(json value) {
  std::lock_guard lock(mutex);
  value["authority_epoch"] = epoch;
  state = std::move(value);
  published = std::chrono::steady_clock::now();
}
json RemoteHost::snapshot() const {
  std::lock_guard lock(mutex);
  return status;
}
void RemoteHost::action(json value) {
  std::lock_guard lock(mutex);
  if (actions.size() < 16)
    actions.push_back(std::move(value));
}
void RemoteHost::takeLocal() {
  std::lock_guard lock(mutex);
  epoch = nonce();
  overridden = true;
  requests.clear();
  actions.push_front({{"op", "control_release"}});
}
std::string RemoteHost::authorityEpoch() const {
  std::lock_guard lock(mutex);
  return epoch;
}
std::vector<RemoteRequest> RemoteHost::drain() {
  std::lock_guard lock(mutex);
  std::vector<RemoteRequest> out(requests.begin(), requests.end());
  requests.clear();
  return out;
}
bool RemoteHost::permitted(const RemoteRequest &r) const {
  std::lock_guard lock(mutex);
  return !overridden && std::chrono::steady_clock::now() < r.expires &&
         r.wire.value("epoch", "") == epoch &&
         status.value("owner", "") == r.wire.value("peer", "") && status.value("host_fresh", false);
}
void RemoteHost::complete(const RemoteRequest &r, json result, json current_state) {
  std::lock_guard lock(mutex);
  current_state["authority_epoch"] = epoch;
  state = std::move(current_state);
  published = std::chrono::steady_clock::now();
  result["id"] = r.wire.at("id");
  result["peer"] = r.wire.at("peer");
  if (results.size() < 32)
    results.push_back(std::move(result));
}
void RemoteHost::run() {
  while (!stopping) {
    try {
      json ipc;
      {
        std::ifstream f(directory / "ipc.json");
        f >> ipc;
      }
      if (process_id && ipc.value("pid", 0ul) != process_id)
        throw std::runtime_error("Servizio pannello in avvio");
      json packet;
      {
        std::lock_guard lock(mutex);
        packet = {{"state", state}, {"actions", actions}, {"results", results}};
        packet["state"]["source_age_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now() - published)
                                               .count();
        actions.clear();
        results.clear();
      }
      if (!packet["state"].empty()) {
        auto answer = post(ipc.at("address"), ipc.at("token"), packet);
        std::lock_guard lock(mutex);
        if (answer.contains("error")) {
          status["error"] = answer.at("error");
        } else {
          status = answer.at("remote");
          if (status.value("owner", "").empty())
            overridden = false;
          for (auto &r : answer.at("requests")) {
            const auto ttl = std::clamp(r.value("ttl_ms", 0), 0, 500);
            if (requests.size() < 8)
              requests.push_back(
                  {r, std::chrono::steady_clock::now() + std::chrono::milliseconds(ttl)});
          }
        }
      }
    } catch (const std::exception &e) {
      std::lock_guard lock(mutex);
      status["error"] = e.what();
      status["host_fresh"] = false;
      status["owner"] = "";
      requests.clear();
    }
    for (int i = 0; i < 10 && !stopping; ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
json remoteState(const Session &s, bool can_remote) {
  json catalog = json::array();
  for (const auto &f : faultCatalog)
    catalog.push_back({{"id", f.id},
                       {"name", f.name},
                       {"min", f.min},
                       {"max", f.max},
                       {"initial", f.initial},
                       {"unit", f.unit},
                       {"description", f.description},
                       {"reversible", f.reversible}});
  json ev = s.eventFrame();
  if (ev["events"].size() > 50)
    ev["events"].erase(ev["events"].begin(), ev["events"].end() - 50);
  return {{"telemetry", s.telemetry()},
          {"alarm_status", s.alarmStatus()},
          {"events", ev},
          {"configuration", s.configuration()},
          {"material", materialInfo(s.config)},
          {"material_catalog", materialCatalog()},
          {"metrics", s.metrics()},
          {"next_command_id", std::to_string(s.nextCommandId())},
          {"fault_catalog", catalog},
          {"can_remote", can_remote},
          {"host_version", SLX_RS_VERSION}};
}
json executeRemote(Session &s, const json &p, const std::filesystem::path &results_root) {
  if (p.at("run_id") != s.telemetry().at("run_id"))
    throw std::runtime_error("La prova destinataria e cambiata");
  const auto op = p.at("op").get<std::string>();
  if (op == "command") {
    const auto c = decodeCommand(p.at("command"), s.telemetry().at("run_id"));
    const auto result = s.submit(c);
    return {{"ok", result == 0},
            {"sdk_result", result},
            {"command_id", std::to_string(c.id)},
            {"state", s.telemetry().at("state")}};
  }
  if (op == "setpoints") {
    const double speed = p.at("speed"), tension = p.at("tension");
    if (!std::isfinite(speed) || speed < 0 || speed > 4 || !std::isfinite(tension) ||
        tension < 20 || tension > 200)
      throw std::runtime_error("Riferimenti fuori campo");
    int a = s.command(SLX_RS_SET_SPEED, speed);
    int b = a ? a : s.command(SLX_RS_SET_TENSION, tension);
    return {{"ok", a == 0 && b == 0}, {"sdk_result", b}};
  }
  if (op == "recipe") {
    auto c = s.config;
    const int id = p.at("id");
    applyMaterialRecipe(c, id);
    s.configure(c);
    return {{"ok", true}};
  }
  if (op == "schedule") {
    s.schedule({p.at("fault"), p.at("at_s"), p.at("duration_s"), p.at("value")});
    return {{"ok", true}};
  }
  if (op == "cancel_schedule") {
    const auto i = p.at("index").get<size_t>();
    if (i >= s.plans.size() || s.plans[i].injected)
      throw std::runtime_error("Prova gia attivata o non disponibile");
    s.plans.erase(s.plans.begin() + i);
    return {{"ok", true}};
  }
  if (op == "export") {
    const auto name = "Pannello-" + nonce();
    s.exportTrial(results_root / name, "panel");
    return {{"ok", true}, {"directory", name}};
  }
  if (op == "length_stop") {
    s.setLengthTarget(p.at("target_m"));
    return {{"ok", true}};
  }
  if (op == "pause_on_alarm") {
    s.setPauseOnAlarm(p.at("enabled"));
    return {{"ok", true}};
  }
  throw std::runtime_error("Operazione remota non disponibile");
}
} // namespace slx::reelsync
