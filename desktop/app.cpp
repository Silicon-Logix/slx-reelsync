// SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#include "alarms.hpp"
#include "faults.hpp"
#include "gl_api.hpp"
#include "platform.hpp"
#include "recipes.hpp"
#include "remote.hpp"
#include "renderer.hpp"
#include "roadmap.hpp"
#include "session.hpp"
#include "ui.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <future>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
using namespace slx::reelsync;
namespace u = slx::reelsync::ui;
namespace {
const char *pageNames[] = {"Panoramica",  "Prova e ricette",  "Regolazione",
                           "Grafici",     "Guasti e allarmi", "Risultati",
                           "Diagnostica", "Su richiesta",     "Pannello Wi-Fi"};
const char *cameraNames[] = {"Linea completa",   "Panoramica ambiente",
                             "Ballerino",        "Riavvolgitore",
                             "Svolgitore",       "HMI",
                             "Quadro elettrico", "Servizi",
                             "Vista frontale",   "Targa SiliconLogiX"};
struct Group {
  const char *id;
  const char *label;
};
constexpr Group groups[] = {{"", "Linea completa"},
                            {"rs_unwinder_root", "Svolgitore"},
                            {"rs_drive_module_root", "Trascinamento"},
                            {"rs_dancer_module_root", "Ballerino"},
                            {"rs_rewinder_root", "Riavvolgitore"},
                            {"rs_hmi_root", "HMI"},
                            {"rs_main_cabinet_root", "Quadro elettrico"}};
std::string eventText(const SLX_ReelSync_Event &e) {
  if (e.type == 105) {
    if (!e.result)
      return "Condizioni di allarme rientrate";
    std::string text = "Allarmi: ";
    for (size_t i = 0; i < alarmNames.size(); ++i)
      if (uint32_t(e.result) & (1u << i)) {
        if (text.size() > 9)
          text += ", ";
        text += alarmNames[i];
      }
    return text;
  }
  if (e.type <= 14 && e.result)
    return "Comando respinto dal core (codice " + std::to_string(e.result) + ")";
  switch (e.type) {
  case 1:
    return "Marcia avviata";
  case 2:
    return "Arresto controllato richiesto";
  case 3:
    return "Analisi in pausa: tempo congelato";
  case 4:
    return "Analisi ripresa";
  case 5:
    return "Nuova prova: condizioni iniziali ripristinate";
  case 6:
    return "Nuovo riferimento velocità: " + std::to_string(e.value) + " m/s";
  case 7:
    return "Nuovo riferimento tensione: " + std::to_string(e.value) + " N";
  case 8:
    return "Manutenzione visuale aperta";
  case 9:
    return "Manutenzione visuale chiusa";
  case 10:
    return std::string("Anomalia iniettata: ") + faultName(e.argument);
  case 11:
    return "Guasto azzerato con nuova prova";
  case 12:
    return "Avanzamento di un passo numerico";
  case 13:
    return "Causa rimossa: stato e dati conservati";
  case 14:
    return "Memoria allarmi confermata";
  case 100:
    return "Ricetta validata: linea pronta";
  case 102:
    return "Uscita controllore non valida: frenatura memorizzata";
  case 103:
    return "Arresto automatico: verificare la diagnosi del processo";
  case 104:
    return "Arresto controllato completato";
  case 106:
    return "Limite del modello raggiunto: avviare una nuova prova";
  default:
    return e.message;
  }
}
std::string number(double v, int decimals, const char *unit = "") {
  std::ostringstream s;
  s << std::fixed << std::setprecision(decimals) << v;
  if (*unit)
    s << ' ' << unit;
  return s.str();
}
std::filesystem::path newDirectory(const std::filesystem::path &parent, const char *prefix) {
  std::filesystem::create_directories(parent);
  for (int i = 1; i < 100000; ++i) {
    std::ostringstream n;
    n << prefix << '-' << std::setfill('0') << std::setw(3) << i;
    auto p = parent / n.str();
    if (std::filesystem::create_directory(p))
      return p;
  }
  throw std::runtime_error("Impossibile creare una nuova cartella di risultati");
}
struct Job {
  std::atomic<int> completed{0}, total{0};
  std::atomic<bool> cancel{false};
};
class Workbench {
  std::filesystem::path root, data;
  GLFWwindow *window;
  bool smoke;

public:
  Session session;
  RemoteHost remote;
  slx::desktop::Renderer renderer;
  slx::desktop::ImageInfo logo, icon;
  SLX_ReelSync_Config draft{};
  int page = 0, camera = 0, group = 0, fault_index = 0, scenario = 1, style = 1;
  bool presentation = false, clip_play = false, hide_guards = false, hide_hall = false,
       shadows = true;
  bool timed = false, error = false, request_about = false, request_help = false;
  float clip = 0, multiplier = 1, exposure = 1.08f, light = 1;
  float speed = 2, tension = 80;
  double severity = 10, onset = 20, duration = 0, debt = 0, core_ms = 0;
  double injection_delay = 10;
  std::string message = "Pronto. Configura una ricetta oppure avvia la linea.", selection;
  u::ChartView charts;
  json bookmarks = json::array(), job_result = json::array();
  char bookmark_name[64] = "Vista operatore";
  std::shared_ptr<Job> job;
  std::future<json> future;
  std::filesystem::path last_export;
  bool camera_smoke = false;
  bool about_visible = false;
  bool web_markers = true;
  float marker_pitch = .5f;
  double target_length = 25;
  json marker_checks;
  json support_checks;
  bool recovery_available = false;
  double checkpoint_wall = 0;
  double remote_wall = .1;
  json instrument_checks;
  json alarm_checks;
  std::string remote_prompt_handled;
  json approval_qualification;
  bool approval_visible_any_page = false;
  Workbench(std::filesystem::path r, GLFWwindow *w, bool test, std::filesystem::path output)
      : root(std::move(r)), data(test ? output / "user-data" : userDataRoot()), window(w),
        smoke(test), session(root / "evaluation.license"), remote(root, data, !test),
        renderer(root / "assets") {
    std::filesystem::create_directories(data);
    draft = session.config;
    logo = renderer.image_info(root / "assets/branding/siliconlogix-logo-light-transparent.png");
    icon = renderer.image_info(root / "assets/branding/reelsync.png");
    renderer.window_icon(window, root / "assets/branding/reelsync.png");
    severity = faultCatalog[0].initial;
    if (!smoke)
      loadPreferences();
    if (!smoke && std::filesystem::is_regular_file(data / "sessione.json")) {
      try {
        if (std::filesystem::file_size(data / "sessione.json") > 32 * 1024 * 1024)
          throw std::runtime_error("Salvataggio troppo grande.");
        std::ifstream f(data / "sessione.json");
        const auto saved = json::parse(f);
        recovery_available = saved.value("final_time_s", 0.) > 0;
      } catch (...) {
        notify("Il salvataggio precedente non e leggibile. Puoi avviare una nuova prova.", true);
      }
    }
  }
  ~Workbench() {
    if (job)
      job->cancel = true;
    if (future.valid())
      future.wait();
    if (!smoke) {
      try {
        savePreferences();
      } catch (...) {
      }
    }
  }
  void notify(std::string text, bool failed = false) {
    message = std::move(text);
    error = failed;
  }
  void checkpoint(bool force = false) {
    if (smoke || (!force && session.snapshot().sim_s <= 0))
      return;
    atomicWrite(data / "sessione.json", session.recording("saved-workspace").dump(2));
    checkpoint_wall = 0;
  }
  void archiveRecovery() {
    if (!recovery_available)
      return;
    auto out = newDirectory(data / "Archivio", "Sessione");
    std::filesystem::copy_file(data / "sessione.json", out / "trial.json");
    recovery_available = false;
  }
  void archiveTrial() {
    if (smoke || session.snapshot().sim_s <= 0)
      return;
    last_export = newDirectory(data / "Archivio", "Prova");
    session.exportTrial(last_export, "automatic-archive");
  }
  void configure(const SLX_ReelSync_Config &configuration) {
    remote.takeLocal();
    archiveRecovery();
    archiveTrial();
    session.configure(configuration);
    checkpoint(true);
  }
  void restoreWorkspace() {
    remote.takeLocal();
    std::ifstream file(data / "sessione.json");
    const auto saved = json::parse(file);
    session.restore(saved);
    auto current = session.snapshot();
    if (current.state == SLX_RS_RUNNING || current.state == SLX_RS_STOPPING ||
        current.state == SLX_RS_FAULT)
      check(session.command(SLX_RS_PAUSE));
    draft = session.config;
    speed = float(current.speed_setpoint_m_s);
    tension = float(current.tension_setpoint_n);
    presentation = false;
    recovery_available = false;
    checkpoint();
    notify("Sessione ripristinata. Premi Riprendi per continuare.");
  }
  void command(uint32_t type, double value = 0, uint32_t arg = 0, bool local_control = true) {
    if (local_control)
      remote.takeLocal();
    try {
      archiveRecovery();
      if (type == SLX_RS_RESET)
        archiveTrial();
    } catch (const std::exception &e) {
      notify(e.what(), true);
      return;
    }
    int result = session.command(type, value, arg);
    if (result) {
      notify(result == SLX_RS_STATE_ERROR
                 ? "Il core ha respinto il comando: verificare stato, causa attiva e condizioni di "
                   "arresto."
                 : "Parametro o comando non valido (SDK " + std::to_string(result) + ").",
             true);
      return;
    }
    if (type == SLX_RS_RESET) {
      recovery_available = false;
      try {
        checkpoint(true);
      } catch (const std::exception &e) {
        notify(e.what(), true);
        return;
      }
    }
    notify("Comando eseguito.");
  }
  json remoteSnapshot() const {
    auto value = remoteState(session, !presentation);
    value["analysis"] = {{"running", bool(job)},
                         {"completed", job ? job->completed.load() : 0},
                         {"total", job ? job->total.load() : 0},
                         {"rows", job_result}};
    return value;
  }
  void tick(double wall) {
    if (!smoke) {
      for (const auto &request : remote.drain()) {
        json result;
        try {
          if (presentation || !remote.permitted(request))
            throw std::runtime_error("Comando scaduto, presentazione attiva o controllo revocato");
          const auto &payload = request.wire.at("payload");
          const auto op = payload.value("op", "");
          if (op == "recipe" ||
              (op == "command" && payload.at("command").value("command", "") == "reset"))
            archiveTrial();
          if (op == "analysis") {
            if (payload.at("run_id") != session.telemetry().at("run_id") || job)
              throw std::runtime_error("Analisi non disponibile per questa prova");
            const auto kind = payload.at("kind").get<std::string>();
            if (kind != "campaign" && kind != "comparison")
              throw std::runtime_error("Tipo analisi non disponibile");
            startJob(kind == "campaign");
            result = {{"ok", true}, {"message", "Analisi avviata sul PC"}};
          } else
            result = executeRemote(session, payload, data / "Risultati");
          if (result.value("ok", false)) {
            draft = session.config;
            speed = float(session.snapshot().speed_setpoint_m_s);
            tension = float(session.snapshot().tension_setpoint_n);
            notify("Richiesta del pannello eseguita e confermata.");
          }
        } catch (const std::exception &e) {
          result = {{"ok", false}, {"error", e.what()}};
        }
        remote.complete(request, result, remoteSnapshot());
      }
      if ((remote_wall += wall) >= .1) {
        remote.publish(remoteSnapshot());
        remote_wall = 0;
      }
    }
    auto start = std::chrono::steady_clock::now();
    auto s = session.snapshot();
    if (!presentation &&
        (s.state == SLX_RS_RUNNING || s.state == SLX_RS_STOPPING || s.state == SLX_RS_FAULT)) {
      debt += wall * multiplier;
      uint32_t count = uint32_t(std::min(400., debt / session.config.dt_s));
      session.step(count);
      debt -= count * session.config.dt_s;
    } else
      debt = 0;
    core_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (presentation && clip_play)
      clip = std::min(60.f, clip + float(wall));
    renderer.update(smoke ? .1 : wall);
    if (!smoke && (checkpoint_wall += wall) >= 5)
      checkpoint();
    if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      try {
        job_result = future.get();
        notify("Analisi completata. Report e tracce disponibili nella cartella risultati.");
      } catch (const std::exception &e) {
        notify(e.what(), true);
      }
      job.reset();
    }
  }
  void savePreferences() {
    json p = {{"schema", "slx.reelsync.workspace.v1"},
              {"bookmarks", bookmarks},
              {"guards_hidden", hide_guards},
              {"hall_hidden", hide_hall},
              {"style", style},
              {"shadows", shadows},
              {"exposure", exposure},
              {"light", light},
              {"web_markers", web_markers},
              {"marker_pitch_m", marker_pitch}};
    std::ofstream f(data / "workspace.json");
    f << p.dump(2);
    if (!f)
      throw std::runtime_error("Preferenze non salvate");
  }
  void loadPreferences() {
    try {
      std::ifstream f(data / "workspace.json");
      if (!f)
        return;
      auto p = json::parse(f);
      if (p.at("schema") != "slx.reelsync.workspace.v1")
        return;
      if (p.contains("bookmarks") && p["bookmarks"].is_array() && p["bookmarks"].size() <= 6)
        bookmarks = p["bookmarks"];
      hide_guards = p.value("guards_hidden", false);
      hide_hall = p.value("hall_hidden", false);
      shadows = p.value("shadows", true);
      style = std::clamp(p.value("style", 1), 0, 1);
      exposure = std::clamp(p.value("exposure", 1.08f), .45f, 1.6f);
      light = std::clamp(p.value("light", 1.f), .35f, 1.8f);
      web_markers = p.value("web_markers", true);
      marker_pitch = std::clamp(p.value("marker_pitch_m", .5f), .25f, 2.f);
    } catch (...) {
      notify("Preferenze precedenti non leggibili: ripristinata la vista iniziale.");
    }
  }
  const char *groupLabel() const {
    for (auto g : groups)
      if (selection == g.id)
        return g.label;
    return "Linea completa";
  }
  json renderState(Session *override_session = nullptr) {
    auto &source = override_session ? *override_session : session;
    auto s = source.snapshot();
    auto d = source.diagnostics();
    auto tracking = source.webTracking();
    json path = json::array(), phases = json::array();
    for (auto p : source.path())
      path.push_back({p.x_m, p.y_m, p.z_m, p.distance_m});
    for (double p : s.phase_rad)
      phases.push_back(p);
    return {{"phase", phases},
            {"alarm_status", source.alarmStatus()},
            {"telemetry", source.telemetry()},
            {"path", path},
            {"ru", s.unwind_radius_m},
            {"rr", s.rewind_radius_m},
            {"dancer_z_m", s.dancer_z_m},
            {"distance_m", s.transported_m},
            {"web_tracking",
             {{"origin_m", tracking.material_origin_m},
              {"upstream_rest_m", tracking.upstream_rest_m},
              {"downstream_rest_m", tracking.downstream_rest_m},
              {"upstream_route_m", tracking.upstream_route_m},
              {"downstream_route_m", tracking.downstream_route_m}}},
            {"width_m", source.config.width_m},
            {"broken", s.fault == SLX_RS_WEB_BREAK},
            {"maintenance", s.state == SLX_RS_MAINTENANCE},
            {"operating_state", s.state == SLX_RS_PAUSED ? d.paused_state : s.state},
            {"selected", selection}};
  }
  void shortcuts() {
    if (ImGui::GetIO().WantTextInput)
      return;
    if (ImGui::IsKeyPressed(ImGuiKey_F12))
      request_about = true;
    if (ImGui::IsKeyPressed(ImGuiKey_F1))
      request_help = true;
    if (ImGui::IsPopupOpen("Informazioni su SLX ReelSync") || ImGui::IsPopupOpen("Guida rapida"))
      return;
    auto s = session.snapshot();
    if (!presentation && ImGui::IsKeyPressed(ImGuiKey_Space)) {
      if (s.state == SLX_RS_READY || s.state == SLX_RS_STOPPED)
        command(SLX_RS_START);
      else if (s.state == SLX_RS_PAUSED)
        command(SLX_RS_RESUME);
      else
        command(SLX_RS_PAUSE);
    }
    if (!presentation && ImGui::IsKeyPressed(ImGuiKey_S) && s.state == SLX_RS_RUNNING)
      command(SLX_RS_STOP);
    if (page == 0 && ImGui::IsKeyPressed(ImGuiKey_Home)) {
      renderer.frame();
      camera = 0;
    }
    if (page == 0 && ImGui::IsKeyPressed(ImGuiKey_F))
      renderer.frame(selection);
  }
  void draw() {
    shortcuts();
    ImVec2 size = ImGui::GetIO().DisplaySize;
    const float k = u::scale();
    float side = 244 * k;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({side, size.y});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, u::rgb(0x111C31));
    ImGui::PushStyleColor(ImGuiCol_Text, u::rgb(0xDCE5F4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("SLX / Navigazione", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetCursorPosX(31 * k);
    const bool compact = size.y / k < 850;
    u::image(logo, (compact ? 124.f : 182.f) * k);
    ImGui::Dummy({0, compact ? 0.f : 12.f * k});
    u::image(icon, 34 * k);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushFont(u::fonts.bold);
    ImGui::TextUnformatted("ReelSync");
    ImGui::PopFont();
    ImGui::PushFont(u::fonts.small);
    ImGui::TextColored(u::rgb(0x9BABC2), "SIMULATION LAB  /  %s", SLX_RS_VERSION);
    ImGui::PopFont();
    ImGui::EndGroup();
    ImGui::Dummy({0, compact ? 0.f : 18.f * k});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {10, (compact ? 5.f : 10.f) * k});
    for (int i = 0; i < 9; ++i) {
      if (u::nav(pageNames[i], i, page == i, side - 44 * k, (compact ? 34.f : 43.f) * k))
        page = i;
      if (i == 4 && (session.diagnostics().active_alarms || session.diagnostics().latched_alarms ||
                     session.snapshot().fault)) {
        auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddCircleFilled({b.x - 12 * k, (a.y + b.y) / 2}, 5 * k,
                                                    session.diagnostics().active_alarms
                                                        ? IM_COL32(230, 55, 65, 255)
                                                        : IM_COL32(245, 170, 40, 255));
      }
    }
    ImGui::PopStyleVar();
    ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY() + 16, size.y - 149 * k));
    ImGui::PushStyleColor(ImGuiCol_Button, u::rgb(0x22314A));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, u::rgb(0x303F59));
    if (ImGui::Button("Informazioni sul prodotto", {side - 44 * k, 36 * k}))
      request_about = true;
    if (ImGui::Button("Guida rapida  /  F1", {side - 44 * k, 32 * k}))
      request_help = true;
    ImGui::PopStyleColor(2);
    ImGui::PushFont(u::fonts.small);
    ImGui::TextColored(u::rgb(0x9BABC2), "PC Windows  /  Wi-Fi opzionale");
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::SetNextWindowPos({side, 0});
    ImGui::SetNextWindowSize({size.x - side, size.y});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("SLX ReelSync", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    auto s = session.snapshot();
    auto d = session.diagnostics();
    bool stopped = s.state == SLX_RS_READY || s.state == SLX_RS_STOPPED;
    unsigned state_color = s.state == SLX_RS_FAULT    ? 0xBE5555
                           : s.state == SLX_RS_PAUSED ? 0xA67F31
                                                      : 0x16866D;
    ImGui::PushFont(u::fonts.bold);
    u::badge(presentation ? "PRESENTAZIONE" : stateName(s.state),
             presentation ? 0xA67F31 : state_color);
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("  %.3f s  /  %s", s.sim_s,
                        session.config.controller == 1 ? "SLX Reference" : "Didattico C");
    const float modes_x = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - 306 * k;
    if (ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + 18 < modes_x)
      ImGui::SameLine(modes_x);
    if (u::modeSelector(presentation,
                        stopped || s.state == SLX_RS_PAUSED || s.state == SLX_RS_MAINTENANCE))
      clip_play = false;
    ImGui::SameLine();
    if (u::infoButton())
      request_about = true;
    if (!presentation) {
      if (u::button("Avvia", stopped, true))
        command(SLX_RS_START);
      ImGui::SameLine();
      bool pause_ok = !(d.active_alarms & SLX_RS_ALARM_MODEL_BOUNDARY) &&
                      (s.state == SLX_RS_RUNNING || s.state == SLX_RS_STOPPING ||
                       s.state == SLX_RS_FAULT || s.state == SLX_RS_PAUSED);
      if (u::button(s.state == SLX_RS_PAUSED ? "Riprendi" : "Pausa analisi", pause_ok))
        command(s.state == SLX_RS_PAUSED ? SLX_RS_RESUME : SLX_RS_PAUSE);
      ImGui::SameLine();
      if (u::button("Un passo", s.state == SLX_RS_PAUSED))
        command(SLX_RS_SINGLE_STEP);
      ImGui::SameLine();
      if (u::button("Arresta", s.state == SLX_RS_RUNNING))
        command(SLX_RS_STOP);
      ImGui::SameLine();
      if (u::button("Nuova prova",
                    stopped || s.state == SLX_RS_FAULT || s.state == SLX_RS_MAINTENANCE))
        command(SLX_RS_RESET);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(116);
      ImGui::SliderFloat("Ritmo richiesto", &multiplier, .2f, 10, "%.1fx");
    } else {
      if (u::button(clip_play ? "Pausa clip" : "Riproduci clip", true, true))
        clip_play = !clip_play;
      ImGui::SameLine();
      ImGui::SetNextItemWidth(450);
      ImGui::SliderFloat("Clip cinematica [s]", &clip, 0, 60, "%.1f / 60");
    }
    ImGui::PushFont(u::fonts.small);
    if ((d.active_alarms & SLX_RS_ALARM_MODEL_BOUNDARY) && !presentation)
      ImGui::TextColored(u::rgb(0xBE5555),
                         "Limite del modello raggiunto. Selezionare Nuova prova.");
    else if (s.state == SLX_RS_PAUSED && !presentation)
      ImGui::TextColored(u::rgb(0xA67F31), "Analisi in pausa");
    else if (d.active_alarms)
      ImGui::TextColored(u::rgb(0xBE5555), "%d allarmi attivi  /  %s",
                         std::popcount(d.active_alarms),
                         s.fault ? faultName(s.fault) : "Diagnosi del processo");
    else if (d.latched_alarms)
      ImGui::TextColored(u::rgb(0xA67F31),
                         "Allarmi in memoria: verificare il ripristino in Guasti e allarmi.");
    else
      ImGui::TextDisabled("Sessione locale  /  Registrazione segnali a 50 Hz");
    ImGui::PopFont();
    ImGui::Spacing();
    alarmBanner();
    ImGui::BeginChild("Area di lavoro", {0, -39}, ImGuiChildFlags_None);
    try {
      switch (page) {
      case 0:
        overview(s);
        break;
      case 1:
        recipes(stopped);
        break;
      case 2:
        control(stopped);
        break;
      case 3:
        graphs();
        break;
      case 4:
        faults(s, d);
        break;
      case 5:
        results();
        break;
      case 6:
        diagnostics(s, d);
        break;
      case 7:
        onRequest();
        break;
      case 8:
        remotePanel();
        break;
      }
    } catch (const std::exception &e) {
      notify(e.what(), true);
    }
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::PushFont(u::fonts.small);
    ImGui::TextColored(u::rgb(error ? 0xBE5555 : 0x65768D), "%s", message.c_str());
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleVar(2);
    if (request_about) {
      ImGui::OpenPopup("Informazioni su SLX ReelSync");
      request_about = false;
    }
    if (request_help) {
      ImGui::OpenPopup("Guida rapida");
      request_help = false;
    }
    about_visible = u::about(root, session, renderer, logo, icon);
    u::help();
    remoteApproval();
  }
  void alarmBanner() {
    if (presentation)
      return;
    const auto a = session.alarmStatus();
    const auto &items = a.at("items"), &pending = a.at("pending");
    const bool active = a.at("active").get<uint32_t>() != 0;
    const auto fault = session.snapshot().fault;
    std::string text = active  ? "ALLARME " + std::to_string(a.at("active_count").get<int>()) +
                                     " | " + items.front().at("code").get<std::string>() + " " +
                                     items.front().at("title").get<std::string>()
                       : fault ? "PROVA ATTIVA | " + std::string(faultName(fault))
                       : !pending.empty()
                           ? "PROVA TRA " + number(pending.front().at("remaining_s"), 1) + " s | " +
                                 pending.front().at("name").get<std::string>()
                       : !items.empty() ? "ALLARMI RIENTRATI | Conferma il ripristino"
                                        : "LINEA REGOLARE | Nessun allarme";
    if (!pending.empty() && active)
      text += " | Prova tra " + number(pending.front().at("remaining_s"), 1) + " s";
    ImGui::PushStyleColor(ImGuiCol_Button, u::rgb(active                      ? 0xA92C3A
                                                  : (fault || !items.empty()) ? 0x855400
                                                  : !pending.empty()          ? 0x245982
                                                                              : 0x176650));
    ImGui::PushStyleColor(ImGuiCol_Text, u::rgb(0xFFFFFF));
    if (ImGui::Button((text + "###GlobalAlarm").c_str(),
                      {std::max(200.f, ImGui::GetContentRegionAvail().x - 190 * u::scale()),
                       32 * u::scale()}))
      page = 4;
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    if (!pending.empty()) {
      if (u::button("Annulla timer")) {
        const size_t i = pending.front().at("index");
        session.plans.erase(session.plans.begin() + i);
      }
    } else if (!a.at("attention_parts").empty()) {
      if (u::button("Mostra componente")) {
        const std::string part = a.at("attention_parts").front();
        selection = part == "loadcell" ? "rs_loadcell_roller"
                    : part == "dancer" ? "rs_dancer_module_root"
                    : part == "web"    ? "rs_nip_module_root"
                                       : "rs_drive_module_root";
        page = 0;
        renderer.frame(selection);
      }
    }
    ImGui::Spacing();
  }
  void remoteApproval() {
    const auto r =
        smoke && !approval_qualification.is_null() ? approval_qualification : remote.snapshot();
    const auto pending = r.value("pending", json::array());
    const auto controls = r.value("control_requests", json::array());
    const bool pairing = !pending.empty();
    const auto request = pairing            ? pending.front()
                         : controls.empty() ? json::object()
                                            : controls.front();
    const auto id = request.value(pairing ? "pin" : "id", "");
    const auto key = std::string(pairing ? "pair:" : "control:") + id;
    if (id.empty())
      remote_prompt_handled.clear();
    const bool active = !id.empty() && key != remote_prompt_handled;
    if (active && !ImGui::IsPopupOpen("Richiesta dal pannello"))
      ImGui::OpenPopup("Richiesta dal pannello");
    ImGui::SetNextWindowSize({560 * u::scale(), 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {.5f, .5f});
    if (ImGui::BeginPopupModal("Richiesta dal pannello", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      approval_visible_any_page |= active && page != 8;
      if (!active) {
        ImGui::CloseCurrentPopup();
      } else {
        ImGui::TextWrapped("%s richiede %s.", request.value("name", "Pannello Raspberry").c_str(),
                           pairing ? "l'associazione" : "il controllo della linea simulata");
        if (pairing) {
          ImGui::TextWrapped("Confronta l'impronta con quella mostrata sulla scheda:");
          ImGui::TextWrapped("%s", id.c_str());
        } else {
          ImGui::TextWrapped(
              "Consenti al pannello di avviare e arrestare la simulazione, modificare i "
              "riferimenti e iniettare anomalie. Un comando locale riprende il controllo.");
          if (presentation)
            ImGui::TextWrapped("Passa alla modalita simulazione per consentire il controllo.");
        }
        ImGui::Spacing();
        if (u::button(pairing ? "Consenti associazione" : "Consenti controllo",
                      pairing || !presentation, true)) {
          remote.action(pairing ? json{{"op", "pair_approve"}, {"pin", id}}
                                : json{{"op", "control_grant"}, {"id", id}});
          remote_prompt_handled = key;
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (u::button(pairing ? "Chiudi" : "Rifiuta")) {
          if (!pairing)
            remote.action({{"op", "control_deny"}, {"id", id}});
          remote_prompt_handled = key;
          ImGui::CloseCurrentPopup();
        }
      }
      ImGui::EndPopup();
    }
  }
  void remotePanel() {
    u::heading("Pannello Raspberry", "Associazione e controllo della dashboard sulla rete locale.");
    const auto r = remote.snapshot();
    if (r.contains("error"))
      ImGui::TextWrapped("%s", r.at("error").get<std::string>().c_str());
    const bool enabled = r.value("enabled", false);
    if (u::button(enabled ? "Disattiva collegamento" : "Abilita collegamento Wi-Fi", true,
                  !enabled))
      remote.action({{"op", "enable"}, {"enabled", !enabled}});
    ImGui::TextWrapped("Porta TLS: 47631. Inserisci sul pannello l'indirizzo di rete di questo PC. "
                       "Il collegamento usa la rete locale e non richiede Internet.");
    const auto pin = r.value("pin", "");
    ImGui::SeparatorText("Identita del PC");
    ImGui::TextWrapped("Impronta SHA-256: %s", pin.c_str());
    if (u::button("Copia impronta", !pin.empty()))
      ImGui::SetClipboardText(pin.c_str());
    if (u::button("Avvia associazione", enabled, true))
      remote.action({{"op", "pair_begin"}});
    const auto code = r.value("pair_code", "");
    if (!code.empty()) {
      ImGui::Text("Codice temporaneo: %s", code.c_str());
      ImGui::TextDisabled(
          "Valido due minuti. Confronta le impronte e conferma entrambi i dispositivi.");
    }
    ImGui::SeparatorText("Richieste di associazione");
    for (const auto &p : r.value("pending", json::array())) {
      ImGui::PushID(p.at("pin").get<std::string>().c_str());
      ImGui::TextWrapped("%s  /  %s", p.at("name").get<std::string>().c_str(),
                         p.at("pin").get<std::string>().c_str());
      if (u::button("Consenti associazione"))
        remote.action({{"op", "pair_approve"}, {"pin", p.at("pin")}});
      ImGui::PopID();
    }
    ImGui::SeparatorText("Autorita di controllo");
    for (const auto &p : r.value("control_requests", json::array())) {
      ImGui::PushID(p.at("id").get<std::string>().c_str());
      ImGui::TextUnformatted(p.at("name").get<std::string>().c_str());
      if (u::button("Assegna controllo al pannello", !presentation, true))
        remote.action({{"op", "control_grant"}, {"id", p.at("id")}});
      ImGui::PopID();
    }
    if (u::button("Prendi controllo sul PC"))
      remote.takeLocal();
    ImGui::TextWrapped("%s", r.value("owner", "").empty()
                                 ? "Controllo locale. Il pannello puo consultare i dati."
                                 : "Controllo assegnato al pannello. Un comando locale riprende "
                                   "immediatamente il controllo sul PC.");
    ImGui::SeparatorText("Pannelli associati");
    for (const auto &p : r.value("peers", json::array())) {
      ImGui::PushID(p.at("pin").get<std::string>().c_str());
      ImGui::Text("%s / %s", p.at("name").get<std::string>().c_str(),
                  p.value("online", false) ? "Collegato" : "Non collegato");
      if (u::button("Revoca associazione"))
        remote.action({{"op", "peer_revoke"}, {"pin", p.at("pin")}});
      ImGui::PopID();
    }
  }
  void overview(const SLX_ReelSync_Snapshot &s) {
    if (recovery_available && s.sim_s == 0) {
      ImGui::TextColored(u::rgb(0x5354E8), "Una sessione salvata e disponibile.");
      if (u::button("Ripristina sessione", true, true))
        restoreWorkspace();
      ImGui::SameLine();
      if (u::button("Avvia una nuova sessione")) {
        archiveRecovery();
        checkpoint(true);
      }
      ImGui::Spacing();
    }
    u::heading("Osserva il processo",
               "Svolgimento, trascinamento e riavvolgimento nello stesso stato di simulazione.");
    if (!presentation) {
      float w = (ImGui::GetContentRegionAvail().x - 30) / 4;
      auto t = number(s.tension_r_n, 2, "N"), v = number(s.speed_m_s, 3, "m/s"),
           r = number(2 * s.unwind_radius_m, 3) + " / " + number(2 * s.rewind_radius_m, 3) + " m",
           z = number(s.dancer_z_m, 4, "m");
      u::card("tensione", "TENSIONE A VALLE", t.c_str(),
              ("Riferimento " + number(s.tension_setpoint_n, 1, "N")).c_str(), 0x5354E8, w);
      ImGui::SameLine();
      u::card("velocità", "VELOCITA PROCESSO", v.c_str(),
              ("Encoder " + number(s.measured_speed_m_s, 3, "m/s")).c_str(), 0x168B83, w);
      ImGui::SameLine();
      u::card("diametri", "DIAMETRI  /  U - R", r.c_str(), "Bobine a nucleo fisso", 0xA36E39, w);
      ImGui::SameLine();
      u::card("ballerino", "QUOTA BALLERINO", z.c_str(), "Corsa 1.05 - 1.35 m", 0x3D6C96, w);
    } else {
      ImGui::TextColored(u::rgb(0xA67F31), "Presentazione della linea");
    }
    ImGui::Spacing();
    ImGui::SetNextItemWidth(185);
    if (ImGui::Combo("##Camera", &camera, cameraNames, 10))
      renderer.focus(camera);
    ImGui::SameLine();
    if (u::button("Inquadra tutto")) {
      renderer.frame();
      camera = 0;
    }
    ImGui::SameLine();
    if (u::button("Inquadra gruppo", !selection.empty()))
      renderer.frame(selection);
    ImGui::SameLine();
    if (ImGui::BeginCombo("##VisteSalvate", "Viste salvate", ImGuiComboFlags_WidthFitPreview)) {
      for (size_t i = 0; i < bookmarks.size(); ++i) {
        ImGui::PushID(int(i));
        if (ImGui::Selectable(bookmarks[i].at("name").get<std::string>().c_str()))
          renderer.camera_pose(bookmarks[i]["pose"]);
        ImGui::PopID();
      }
      if (bookmarks.empty())
        ImGui::TextDisabled("Nessuna vista salvata");
      ImGui::Separator();
      ImGui::InputText("Nome", bookmark_name, sizeof(bookmark_name));
      if (u::button("Salva vista corrente", bookmarks.size() < 6)) {
        bookmarks.push_back({{"name", bookmark_name}, {"pose", renderer.camera_pose()}});
        savePreferences();
        notify("Vista salvata nel profilo utente.");
      }
      if (u::button("Rimuovi viste", !bookmarks.empty())) {
        bookmarks = json::array();
        savePreferences();
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (u::button("Vista e luci"))
      ImGui::OpenPopup("ViewSettings");
    u::nextInline(ImGui::CalcTextSize("Vista ispezione").x + 42 * u::scale());
    u::toggle("Vista ispezione", &hide_guards);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip(
          "Nasconde le protezioni soltanto nella vista. Non comanda le aperture di manutenzione.");
    if (ImGui::BeginPopup("ViewSettings")) {
      ImGui::TextUnformatted("Aspetto della scena");
      u::toggle("Nascondi protezioni per ispezione", &hide_guards);
      u::toggle("Isola la linea dall'ambiente", &hide_hall);
      u::toggle("Ombre", &shadows);
      ImGui::SeparatorText("Leggibilita del materiale");
      u::toggle("Riferimenti sulla lastra", &web_markers);
      ImGui::SliderFloat("Passo riferimenti [m]", &marker_pitch, .25f, 2.f, "%.2f");
      ImGui::TextWrapped("Regola la distanza tra i riferimenti visivi sulla lastra.");
      ImGui::Combo("Materiali", &style, "Originali\0Contrasto industriale\0");
      ImGui::SliderFloat("Esposizione", &exposure, .45f, 1.6f, "%.2f");
      ImGui::SliderFloat("Luce principale", &light, .35f, 1.8f, "%.2f");
      if (u::button("Ripristina aspetto")) {
        style = 1;
        hide_guards = true;
        hide_hall = false;
        exposure = 1.08f;
        light = 1;
        shadows = true;
        web_markers = true;
        marker_pitch = .5f;
      }
      ImGui::EndPopup();
    }
    renderer.appearance(style, hide_guards, hide_hall, shadows, light);
    renderer.webMarkers(web_markers, marker_pitch);
    renderer.exposure(exposure);
    auto available = ImGui::GetContentRegionAvail();
    float height = std::max(120.f * u::scale(), available.y - 37 * u::scale());
    unsigned texture =
        renderer.render(int(available.x), int(height), renderState(), false, presentation, clip);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddImageRounded((ImTextureID)(intptr_t)texture, p,
                                                {p.x + available.x, p.y + height}, {0, 1}, {1, 0},
                                                IM_COL32_WHITE, 9);
    ImGui::InvisibleButton("Scena 3D", {available.x, height},
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    auto &io = ImGui::GetIO();
    bool hover = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    if (active) {
      if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 2) ||
          (io.KeyShift && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2)))
        renderer.pan(io.MouseDelta.x, io.MouseDelta.y);
      else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3) ||
               ImGui::IsMouseDragging(ImGuiMouseButton_Right, 3))
        renderer.orbit(io.MouseDelta.x, io.MouseDelta.y);
    }
    if (hover) {
      if (io.MouseWheel)
        renderer.zoom(io.MouseWheel);
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        selection =
            renderer.pick((io.MousePos.x - p.x) / available.x, (io.MousePos.y - p.y) / height);
        renderer.frame(selection);
      } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        auto delta = ImGui::GetMouseDragDelta();
        if (delta.x * delta.x + delta.y * delta.y < 9)
          selection =
              renderer.pick((io.MousePos.x - p.x) / available.x, (io.MousePos.y - p.y) / height);
      }
    }
    ImGui::PushFont(u::fonts.small);
    ImGui::TextDisabled("Orbita: trascina  |  Pan: centrale / Maiusc  |  Zoom: rotella  |  F: "
                        "gruppo  |  Home: linea");
    ImGui::SameLine();
    ImGui::Text(" / %s", groupLabel());
    ImGui::PopFont();
  }
  void recipes(bool stopped) {
    u::heading("Prepara una prova", "Imposta materiale, processo e condizioni di misura.");
    if (u::button("Apri ricetta", stopped)) {
      void *owner = nullptr;
#ifdef _WIN32
      owner = glfwGetWin32Window(window);
#endif
      if (auto p = recipeDialog(owner, false)) {
        std::ifstream f(*p);
        auto loaded = configurationFrom(json::parse(f), draft);
        configure(loaded);
        draft = loaded;
        speed = float(draft.speed_m_s);
        tension = float(draft.tension_n);
        notify("Ricetta caricata e validata dal core.");
      }
    }
    ImGui::SameLine();
    if (u::button("Salva ricetta")) {
      void *owner = nullptr;
#ifdef _WIN32
      owner = glfwGetWin32Window(window);
#endif
      if (auto p = recipeDialog(owner, true)) {
        std::ofstream f(*p);
        f << json({{"schema", "slx.reelsync.recipe.v1"}, {"configuration", configurationOf(draft)}})
                 .dump(2);
        if (!f)
          throw std::runtime_error("Ricetta non salvata");
        notify("Ricetta salvata.");
      }
    }
    ImGui::SameLine();
    if (u::button("Applica alla prova", stopped, true)) {
      configure(draft);
      speed = float(draft.speed_m_s);
      tension = float(draft.tension_n);
      notify("Configurazione accettata. Prova pronta.");
    }
    ImGui::BeginDisabled(!stopped);
    if (ImGui::BeginTable("RecipeColumns", 2, ImGuiTableFlags_SizingStretchSame)) {
      ImGui::TableNextColumn();
      ImGui::SeparatorText("Materiale e bobine");
      const int material_id = materialRecipeId(draft);
      ImGui::SetNextItemWidth(270);
      if (ImGui::BeginCombo("Materiale", material_id < 0
                                             ? "Personalizzato"
                                             : materialRecipes[size_t(material_id)].name)) {
        for (size_t id = 0; id < materialRecipes.size(); ++id) {
          const auto &m = materialRecipes[id];
          if (ImGui::Selectable(m.name, int(id) == material_id))
            applyMaterialRecipe(draft, int(id));
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", m.note);
        }
        ImGui::EndCombo();
      }
      ImGui::SetNextItemWidth(180);
      ImGui::InputDouble("Larghezza [m]", &draft.width_m, .05, .1, "%.3f");
      ImGui::SetNextItemWidth(180);
      ImGui::InputDouble("Spessore [m]", &draft.thickness_m, .000005, .00005, "%.8f");
      ImGui::SetNextItemWidth(180);
      ImGui::InputDouble("Modulo [Pa]", &draft.young_pa, 1e6, 1e7, "%.0f");
      ImGui::SetNextItemWidth(180);
      ImGui::InputDouble("Densita [kg/m3]", &draft.density_kg_m3, 10, 100, "%.0f");
      ImGui::SetNextItemWidth(180);
      ImGui::InputDouble("Raggio svolgitore [m]", &draft.unwind_radius_m, .01, .05, "%.3f");
      ImGui::SetNextItemWidth(180);
      ImGui::InputDouble("Raggio riavvolgitore [m]", &draft.rewind_radius_m, .01, .05, "%.3f");
      ImGui::TableNextColumn();
      ImGui::SeparatorText("Processo e azionamenti");
      ImGui::SetNextItemWidth(180);
      ImGui::InputDouble("Velocità [m/s]", &draft.speed_m_s, .1, .5, "%.2f");
      ImGui::SetNextItemWidth(180);
      ImGui::InputDouble("Tensione [N]", &draft.tension_n, 5, 20, "%.1f");
      ImGui::SetNextItemWidth(180);
      ImGui::InputDouble("Accelerazione [m/s2]", &draft.acceleration_m_s2, .05, .1, "%.2f");
      ImGui::SetNextItemWidth(180);
      ImGui::InputDouble("Coppia massima [Nm]", &draft.torque_limit_nm, 10, 100, "%.0f");
      ImGui::SetNextItemWidth(180);
      ImGui::InputDouble("Massa ballerino [kg]", &draft.dancer_mass_kg, 1, 5, "%.1f");
      ImGui::EndTable();
    }
    ImGui::SeparatorText("Sensori e ripetibilità");
    ImGui::SetNextItemWidth(165);
    ImGui::InputDouble("Rumore +/- [N]", &draft.sensor_noise_n, .1, 1, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(165);
    ImGui::InputDouble("Bias [N]", &draft.sensor_bias_n, 1, 5, "%.1f");
    ImGui::SetNextItemWidth(165);
    ImGui::InputDouble("Ritardo [s]", &draft.sensor_delay_s, .005, .005, "%.3f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(165);
    ImGui::InputScalar("Seed", ImGuiDataType_U64, &draft.seed);
    int dt = draft.dt_s == .001 ? 0 : draft.dt_s == .0005 ? 1 : 2;
    ImGui::SetNextItemWidth(165);
    if (ImGui::Combo("Passo numerico [s]", &dt, "0.001\0 0.0005\0 0.00025\0"))
      draft.dt_s = dt == 0 ? .001 : dt == 1 ? .0005 : .00025;
    ImGui::EndDisabled();
    ImGui::Spacing();
    ImGui::TextDisabled(
        "Larghezza 0.20 - 0.80 m  /  Raggio massimo 0.62 m  /  Corsa ballerino 1.05 - 1.35 m");
  }
  void startJob(bool campaign) {
    if (job)
      return;
    job = std::make_shared<Job>();
    job->total = campaign ? int(scenarioCatalog.size()) : 2;
    auto progress = job;
    auto cfg = session.config;
    auto scenario_id = std::string(scenarioCatalog[scenario].id);
    auto application_root = root;
    auto out = newDirectory(data / "Risultati", campaign ? "Campagna" : "Confronto");
    last_export = out;
    future = std::async(
        std::launch::async, [progress, cfg, scenario_id, application_root, out, campaign]() {
          json rows = json::array();
          for (int i = 0; i < progress->total; ++i) {
            if (progress->cancel)
              throw std::runtime_error("Analisi interrotta dall'operatore");
            auto s = campaign ? std::string(scenarioCatalog[size_t(i)].id) : scenario_id;
            Session trial(application_root / "evaluation.license");
            auto c = cfg;
            if (!campaign)
              c.controller = uint32_t(i + 1);
            trial.configure(c);
            runScenario(trial, s, 60, &progress->cancel);
            auto folder = out / (campaign ? s : (i == 0 ? "reference" : "educational"));
            trial.exportTrial(folder, s);
            rows.push_back({{"scenario", s},
                            {"controller", c.controller == 1 ? "SLX Reference" : "Didattico C"},
                            {"state", stateName(trial.snapshot().state)},
                            {"metrics", trial.metrics()}});
            ++progress->completed;
          }
          std::ofstream f(out / "comparison.json");
          f << json({{"schema", "slx.reelsync.comparison.v1"}, {"trials", rows}}).dump(2);
          return rows;
        });
    notify("Analisi avviata su istanze indipendenti. La prova aperta conserva il proprio stato.");
  }
  void jobStatus() {
    if (job) {
      ImGui::ProgressBar(float(job->completed.load()) / std::max(1, job->total.load()), {-140, 24});
      ImGui::SameLine();
      if (u::button("Interrompi analisi"))
        job->cancel = true;
    }
  }
  void comparisonTable() {
    if (job_result.empty())
      return;
    ImGui::SeparatorText("Prove completate");
    if (ImGui::BeginTable("Comparisons", 6,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                              ImGuiTableFlags_SizingStretchProp)) {
      for (auto text : {"Scenario", "Controllore", "RMS [N]", "Max [N]", "Saturazione", "Esito"})
        ImGui::TableSetupColumn(text);
      ImGui::TableHeadersRow();
      for (const auto &r : job_result) {
        const auto &m = r["metrics"];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r["scenario"].get<std::string>().c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r["controller"].get<std::string>().c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", m["tension_rms_error_n"].get<double>());
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", m["tension_max_abs_error_n"].get<double>());
        ImGui::TableNextColumn();
        ImGui::Text("%.2f %%", 100 * m["saturated_sample_fraction"].get<double>());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r["state"].get<std::string>().c_str());
      }
      ImGui::EndTable();
    }
  }
  void control(bool stopped) {
    u::heading("Regolazione e confronto",
               "Riferimenti, risposta e controllori confrontati sulla stessa prova.");
    int controller = int(draft.controller - 1);
    ImGui::BeginDisabled(!stopped);
    ImGui::SetNextItemWidth(370);
    if (ImGui::Combo("Controllore", &controller,
                     "SLX Reference / core proprietario\0Didattico P / modulo C modificabile\0")) {
      draft.controller = uint32_t(controller + 1);
      configure(draft);
      notify("Controllore applicato.");
    }
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(420);
    ImGui::SliderFloat("Velocità [m/s]", &speed, 0, 4, "%.2f");
    ImGui::SameLine();
    if (u::button("Invia velocità", !presentation, true))
      command(SLX_RS_SET_SPEED, speed);
    ImGui::SetNextItemWidth(420);
    ImGui::SliderFloat("Tensione [N]", &tension, 20, 200, "%.1f");
    ImGui::SameLine();
    if (u::button("Invia tensione", !presentation, true))
      command(SLX_RS_SET_TENSION, tension);
    ImGui::SeparatorText("Strumenti operatore");
    ImGui::SetNextItemWidth(200);
    ImGui::InputDouble("Obiettivo metratura [m]", &target_length, 1, 10, "%.2f");
    ImGui::SameLine();
    if (u::button("Arma arresto a metratura", !presentation, true)) {
      session.setLengthTarget(target_length);
      notify("Arresto a metratura armato; anticipo nominale sulla rampa configurata.");
    }
    ImGui::SameLine();
    if (u::button("Disattiva", session.length_target_m > 0))
      session.setLengthTarget(0);
    bool pause_alarm = session.pause_on_alarm;
    if (u::toggle("Pausa analisi al primo allarme attivo (riarma)", &pause_alarm))
      session.setPauseOnAlarm(pause_alarm);
    if (session.analysis_trigger_alarms) {
      ImGui::TextColored(u::rgb(0x985000),
                         "Trigger acquisito: 0x%02X. Disattiva e riattiva per riarmare.",
                         session.analysis_trigger_alarms);
    }
    auto current = session.snapshot();
    if (session.length_target_m > 0)
      ImGui::Text("Materiale: %.3f m  /  obiettivo: %.3f m  /  scarto: %+.3f m",
                  current.transported_m, session.length_target_m,
                  current.transported_m - session.length_target_m);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Il report riporta la metratura raggiunta e lo scarto dall'obiettivo.");
    ImGui::SeparatorText("Scenari e confronto");
    ImGui::SetNextItemWidth(300);
    if (ImGui::BeginCombo("Scenario", scenarioCatalog[scenario].name)) {
      for (size_t i = 0; i < scenarioCatalog.size(); ++i)
        if (ImGui::Selectable(scenarioCatalog[i].name, int(i) == scenario))
          scenario = int(i);
      ImGui::EndCombo();
    }
    ImGui::TextDisabled("%s", scenarioCatalog[scenario].description);
    if (u::button("Confronta i due controllori", !job, true))
      startJob(false);
    ImGui::SameLine();
    if (u::button("Campagna completa guasti", !job))
      startJob(true);
    ImGui::SameLine();
    if (u::button("Apri risultati", !last_export.empty()))
      openDirectory(last_export);
    jobStatus();
    comparisonTable();
    if (!presentation)
      u::chart(session, "control-tension", "Risposta di tensione", "N",
               {{"Richiesta", [](auto &s) { return s.tension_setpoint_n; }, 0x7778CB},
                {"Calcolata", [](auto &s) { return s.tension_r_n; }, 0x188D86}},
               charts, 245, 4);
  }
  void graphs() {
    u::heading("Segnali sincronizzati",
               "Tensione, velocità, diametri, coppie e posizione del ballerino nel tempo.");
    if (presentation) {
      ImGui::TextWrapped("Passa a Simulazione per analizzare i segnali della prova.");
      return;
    }
    u::toggle("Scala automatica", &charts.follow);
    ImGui::SameLine();
    u::toggle("Intera prova", &charts.full_history);
    ImGui::SameLine();
    u::toggle("Marcatori guasti", &charts.show_events);
    if (!charts.full_history) {
      ImGui::SameLine();
      float seconds = float(charts.window_s);
      ImGui::SetNextItemWidth(120);
      if (ImGui::SliderFloat("Finestra [s]", &seconds, 5, 120, "%.0f"))
        charts.window_s = seconds;
    }
    if (ImGui::BeginTable("SignalGrid", 2, ImGuiTableFlags_SizingStretchSame)) {
      ImGui::TableNextColumn();
      u::chart(session, "tension", "Tensione del materiale", "N",
               {{"Richiesta", [](auto &s) { return s.tension_setpoint_n; }, 0x7878BC},
                {"Monte", [](auto &s) { return s.tension_u_n; }, 0xB27842},
                {"Valle", [](auto &s) { return s.tension_r_n; }, 0x168D82},
                {"Sensore", [](auto &s) { return s.measured_tension_n; }, 0xC75B65}},
               charts, 235, 4);
      ImGui::TableNextColumn();
      u::chart(session, "speed", "Velocità ed encoder", "m/s",
               {{"Richiesta", [](auto &s) { return s.speed_setpoint_m_s; }, 0x7778BC},
                {"Processo", [](auto &s) { return s.speed_m_s; }, 0x168D82},
                {"Encoder", [](auto &s) { return s.measured_speed_m_s; }, 0xD18D3E}},
               charts, 235, .3);
      ImGui::TableNextColumn();
      u::chart(session, "diameters", "Diametri bobine", "m",
               {{"Svolgitore", [](auto &s) { return 2 * s.unwind_radius_m; }, 0xB27842},
                {"Riavvolgitore", [](auto &s) { return 2 * s.rewind_radius_m; }, 0x188D86}},
               charts, 230, .02);
      ImGui::TableNextColumn();
      u::chart(session, "torques", "Coppie azionamenti", "Nm",
               {{"Svolgitore", [](auto &s) { return s.unwind_torque_nm; }, 0xB27842},
                {"Riavvolgitore", [](auto &s) { return s.rewind_torque_nm; }, 0x188D86},
                {"Nip", [](auto &s) { return s.nip_torque_nm; }, 0x5F6CC2}},
               charts, 230, 4);
      ImGui::TableNextColumn();
      u::chart(session, "dancer", "Posizione ballerino", "z [m]",
               {{"Quota", [](auto &s) { return s.dancer_z_m; }, 0x3D719D}}, charts, 220, .02);
      ImGui::TableNextColumn();
      u::chart(session, "encoder-error", "Residuo encoder - processo", "m/s",
               {{"Errore di misura", [](auto &s) { return s.measured_speed_m_s - s.speed_m_s; },
                 0xD18D3E}},
               charts, 220, .1);
      ImGui::EndTable();
    }
    ImGui::PushFont(u::fonts.small);
    ImGui::TextDisabled("Disattiva Scala automatica per zoom e pan sui grafici. Le grandezze sono "
                        "registrate a 50 Hz.");
    ImGui::PopFont();
  }
  void faults(const SLX_ReelSync_Snapshot &s, const SLX_ReelSync_Diagnostics &d) {
    u::heading("Allarmi e laboratorio",
               "Cause fisiche e misure alterate, con durata, ripristino e memoria degli allarmi.");
    const auto annunciation = session.alarmStatus();
    for (const auto &alarm : annunciation.at("items")) {
      ImGui::PushStyleColor(ImGuiCol_Text,
                            u::rgb(alarm.at("active").get<bool>() ? 0xB32338 : 0x885700));
      ImGui::TextWrapped("%s | %s | %s | %s", alarm.at("code").get<std::string>().c_str(),
                         alarm.at("title").get<std::string>().c_str(),
                         alarm.at("component").get<std::string>().c_str(),
                         alarm.at("active").get<bool>() ? "ATTIVO" : "RIENTRATO / DA CONFERMARE");
      ImGui::PopStyleColor();
      ImGui::TextWrapped("%s", alarm.at("action").get<std::string>().c_str());
    }
    ImGui::Separator();
    if (ImGui::BeginTable("FaultColumns", 2, ImGuiTableFlags_SizingStretchSame)) {
      ImGui::TableNextColumn();
      ImGui::SeparatorText("Prepara un'anomalia");
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##Fault", faultCatalog[fault_index].name)) {
        for (size_t i = 0; i < faultCatalog.size(); ++i)
          if (ImGui::Selectable(faultCatalog[i].name, int(i) == fault_index)) {
            fault_index = int(i);
            severity = faultCatalog[i].initial;
            if (!faultCatalog[i].reversible)
              duration = 0;
          }
        ImGui::EndCombo();
      }
      const auto &spec = faultCatalog[fault_index];
      ImGui::TextWrapped("%s", spec.description);
      ImGui::Spacing();
      if (spec.max > spec.min) {
        ImGui::SetNextItemWidth(185);
        ImGui::InputDouble(spec.unit, &severity, spec.step, spec.step * 5, "%.3f");
        ImGui::TextDisabled("Campo %.3f - %.3f", spec.min, spec.max);
      }
      u::toggle("Iniezione programmata", &timed);
      if (timed) {
        ImGui::SetNextItemWidth(185);
        ImGui::InputDouble("Inizio [s simulati]", &onset, 1, 5, "%.2f");
      } else {
        ImGui::SetNextItemWidth(185);
        ImGui::InputDouble("Attiva tra [s di prova]", &injection_delay, 1, 5, "%.1f");
        ImGui::TextDisabled("La pausa congela anche il conto alla rovescia.");
      }
      ImGui::BeginDisabled(!spec.reversible);
      ImGui::SetNextItemWidth(185);
      ImGui::InputDouble("Durata [s] / 0 = persistente", &duration, 1, 5, "%.2f");
      ImGui::EndDisabled();
      bool can_schedule = !presentation && (s.state == SLX_RS_READY || s.state == SLX_RS_RUNNING ||
                                            s.state == SLX_RS_STOPPED);
      if (u::button(timed ? "Programma anomalia" : "Avvia conto alla rovescia",
                    can_schedule && !s.fault && (timed || s.state == SLX_RS_RUNNING), true)) {
        if (!std::isfinite(injection_delay) || injection_delay < 0 || injection_delay > 60)
          throw std::runtime_error("Ritardo ammesso: 0 .. 60 s di prova");
        session.schedule({spec.id, timed ? onset : s.sim_s + injection_delay,
                          spec.reversible ? duration : 0, severity});
        notify("Prova programmata. Il conto alla rovescia rimane visibile in tutte le pagine.");
        page = 0;
      }
      ImGui::TableNextColumn();
      ImGui::SeparatorText("Diagnosi e ripristino");
      ImGui::Text("Causa: %s", faultName(s.fault));
      ImGui::Text("Qualità segnali: %s", s.sensor_quality == 1   ? "valida"
                                         : s.sensor_quality == 2 ? "alterata"
                                                                 : "non valida");
      ImGui::Text("Residuo encoder: %.4f m/s", d.encoder_error_m_s);
      ImGui::Text("Utilizzo coppia disponibile: %.1f %%", d.torque_utilization * 100);
      u::alarms(d.active_alarms, d.latched_alarms);
      if (u::button("Rimuovi causa", s.fault != 0 && s.fault != SLX_RS_WEB_BREAK && !presentation))
        command(SLX_RS_REMOVE_FAULT, 0, 0, false);
      ImGui::SameLine();
      ImGui::TextWrapped("%s", annunciation.at("recovery_hint").get<std::string>().c_str());
      if (u::button("Conferma ripristino",
                    annunciation.at("can_acknowledge").get<bool>() && !presentation))
        command(SLX_RS_ACKNOWLEDGE);
      ImGui::TextWrapped("La rimozione ripristina il parametro guasto e conserva dati e tempo. Una "
                         "fermata memorizzata richiede condizioni stabili e conferma. La rottura "
                         "del materiale richiede una nuova prova.");
      ImGui::EndTable();
    }
    if (!session.plans.empty()) {
      ImGui::SeparatorText("Sequenza programmata");
      int remove = -1;
      if (ImGui::BeginTable("Plans", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        for (auto label : {"Anomalia", "Inizio [s]", "Durata [s]", "Stato", ""})
          ImGui::TableSetupColumn(label);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < session.plans.size(); ++i) {
          auto &p = session.plans[i];
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(faultName(p.fault));
          ImGui::TableNextColumn();
          ImGui::Text("%.2f", p.at_s);
          ImGui::TableNextColumn();
          ImGui::Text("%.2f", p.duration_s);
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(!p.injected          ? "In attesa"
                                 : p.injection_result ? "Iniezione respinta"
                                 : p.removed
                                     ? (p.removal_result ? "Ripristino respinto" : "Causa rimossa")
                                     : "Iniettata");
          ImGui::TableNextColumn();
          ImGui::PushID(int(i));
          if (u::button("Elimina", !p.injected))
            remove = int(i);
          ImGui::PopID();
        }
        ImGui::EndTable();
      }
      if (remove >= 0)
        session.plans.erase(session.plans.begin() + remove);
    }
    if (ImGui::CollapsingHeader("Esercitazioni operatore")) {
      if (u::button("Riferimento fuori campo / 5 m/s", !presentation))
        command(SLX_RS_SET_SPEED, 5);
      ImGui::SameLine();
      if (u::button("Apertura manutenzione in marcia", s.state == SLX_RS_RUNNING && !presentation))
        command(SLX_RS_ENTER_MAINTENANCE);
      ImGui::TextWrapped(
          "Invia un comando errato e osserva la risposta del sistema nella cronologia.");
    }
    ImGui::SeparatorText("Manutenzione visuale");
    if (u::button("Apri protezioni e supporti",
                  (s.state == SLX_RS_READY || s.state == SLX_RS_STOPPED) && !presentation)) {
      command(SLX_RS_ENTER_MAINTENANCE);
      hide_guards = false;
    }
    ImGui::SameLine();
    if (u::button("Chiudi manutenzione", s.state == SLX_RS_MAINTENANCE))
      command(SLX_RS_EXIT_MAINTENANCE);
    ImGui::TextDisabled("Aperture disponibili a linea ferma.");
    ImGui::SeparatorText("Cronologia eventi");
    if (ImGui::BeginTable("Events", 4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("t [s]", 0, .5f);
      ImGui::TableSetupColumn("ID", 0, .4f);
      ImGui::TableSetupColumn("Codice", 0, .4f);
      ImGui::TableSetupColumn("Evento", 0, 4);
      ImGui::TableHeadersRow();
      for (auto i = session.events.rbegin(); i != session.events.rend(); ++i) {
        ImGui::TableNextRow();
        if (i->type == 105 && i->result)
          ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 220, 225, 255));
        else if (i->type == 10)
          ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 235, 200, 255));
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", i->sim_s);
        ImGui::TableNextColumn();
        ImGui::Text("%llu", (unsigned long long)i->command_id);
        ImGui::TableNextColumn();
        if (i->type == 105)
          ImGui::Text("0x%02X", unsigned(i->result));
        else
          ImGui::Text("%d", i->result);
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", eventText(*i).c_str());
      }
      ImGui::EndTable();
    }
  }
  void results() {
    u::heading("Risultati e tracciabilita",
               "Analizza la prova e salva segnali, eventi e risultati.");
    if (u::button("Esporta la prova", !presentation, true)) {
      auto out = newDirectory(data / "Risultati", "Prova");
      session.exportTrial(out, "interactive");
      last_export = out;
      notify("Esportati segnali, eventi, metriche e telemetria.");
    }
    ImGui::SameLine();
    if (u::button("Apri ultima esportazione", !last_export.empty()))
      openDirectory(last_export);
    ImGui::SameLine();
    if (u::button("Cartella risultati"))
      openDirectory(data / "Risultati");
    u::nextInline(180 * u::scale());
    if (u::button("Archivio automatico"))
      openDirectory(data / "Archivio");
    if (u::button("Salva sessione", !presentation && session.snapshot().sim_s > 0)) {
      checkpoint();
      notify("Sessione salvata.");
    }
    u::metricTable(session.metrics());
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Come leggere le metriche"))
      ImGui::TextWrapped(
          "Assestamento: ingresso nella banda +/-2%% del riferimento finale, mantenuta fino al "
          "termine per almeno 1 s. Non osservato indica una prova insufficiente o fuori banda. "
          "Saturazione: quota dei campioni, non durata esatta degli impulsi.");
    ImGui::SeparatorText("Campagna e confronti");
    if (u::button("Esegui 12 scenari", !job))
      startJob(true);
    jobStatus();
    comparisonTable();
  }
  void diagnostics(const SLX_ReelSync_Snapshot &s, const SLX_ReelSync_Diagnostics &d) {
    u::heading("Stato della sessione", "Processo, segnali e prestazioni dell'applicazione.");
    const auto r = renderer.diagnostics();
    ImGui::SeparatorText("Processo");
    ImGui::Text("Stato: %s", stateName(s.state));
    ImGui::Text("Tempo: %.3f s", s.sim_s);
    ImGui::Text("Campioni registrati: %zu", session.history.size());
    ImGui::Text("Materiale al nip: %.3f m", s.transported_m);
    ImGui::Text("Bilancio materiale: %.3g m", s.material_balance_error_m);
    ImGui::Text("Segnali: %s", s.sensor_quality == 1   ? "Nominali"
                               : s.sensor_quality == 2 ? "Alterati"
                                                       : "Non disponibili");
    ImGui::SeparatorText("Allarmi");
    u::alarms(d.active_alarms, d.latched_alarms);
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Dettagli di sistema")) {
      ImGui::Text("Versione: %s", SLX_RS_VERSION);
      ImGui::Text("Core: %s  /  SDK ABI: %u", session.build.core_version, SLX_RS_ABI);
      ImGui::Text("Modello: %s", session.build.model_version);
      ImGui::Text("Build: %s", session.build.build_id);
      ImGui::Text("Passo di calcolo: %.6f s  /  Seed: %llu", session.config.dt_s,
                  (unsigned long long)session.config.seed);
      ImGui::TextWrapped("GPU: %s", r.value("gl_renderer", std::string()).c_str());
      ImGui::Text("Rendering: %.2f ms  /  Simulazione: %.2f ms", r.value("cpu_submit_ms", 0.),
                  core_ms);
      ImGui::Text("Frame: %.2f ms", 1000. / std::max(1.f, ImGui::GetIO().Framerate));
      ImGui::Text("Caricamento scena: %.1f ms", r.value("load_ms", 0.));
    }
    ImGui::Spacing();
    if (u::button("Informazioni sul prodotto"))
      request_about = true;
  }
  void setupSmoke() {
    check(session.command(SLX_RS_START));
    session.step(24000);
    check(session.command(SLX_RS_INJECT_FAULT, 1.25, SLX_RS_ENCODER_SCALE));
    session.step(2000);
    check(session.command(SLX_RS_PAUSE));
  }
  void onRequest() {
    u::heading("Funzioni avanzate su richiesta", "Estensioni personalizzate per il tuo processo.");
    u::badge("SVILUPPO SU RICHIESTA", 0x5354E8);
    ImGui::TextWrapped(
        "Scegli un ambito per conoscere obiettivi e dati necessari a sviluppare l'estensione.");
    static int selected = 0;
    ImGui::Spacing();
    {
      ImGui::BeginChild("ExtensionList", {315, 350});
      for (size_t i = 0; i < extensions.size(); ++i) {
        if (u::button(extensions[i].name, true, selected == int(i), {300, 54}))
          selected = int(i);
      }
      ImGui::EndChild();
      ImGui::SameLine();
      ImGui::BeginChild("ExtensionDetail", {0, 350}, ImGuiChildFlags_AlwaysUseWindowPadding);
      const auto &e = extensions[selected];
      ImGui::PushFont(u::fonts.bold);
      ImGui::TextWrapped("%s", e.name);
      ImGui::PopFont();
      ImGui::TextWrapped("%s", e.benefit);
      ImGui::SeparatorText("Ambito proposto");
      ImGui::TextWrapped("%s", e.scope);
      ImGui::SeparatorText("Dati necessari");
      ImGui::TextWrapped("%s", e.inputs);
      ImGui::SeparatorText("Riferimento tecnico");
      ImGui::TextWrapped("%s", e.source);
      if (u::button("Apri fonte del produttore"))
        openReference(e.url);
      ImGui::EndChild();
    }
    ImGui::SeparatorText("Funzioni incluse");
    ImGui::TextWrapped(
        "Ricette, confronto controllori, 12 scenari, guasti temporizzati e recupero, replay "
        "deterministico, esportazione CSV, arresto a metratura e trigger di pausa analisi.");
    if (u::button("Copia scheda per definire la richiesta", true, true)) {
      const auto &e = extensions[selected];
      std::string text =
          std::string("SLX ReelSync / richiesta di sviluppo\nAmbito: ") + e.name +
          "\nObiettivo: " + e.benefit + "\nDati necessari: " + e.inputs +
          "\nLinea e materiale: [da definire]\nCriteri di accettazione: [da definire]\nFonte: " +
          e.url;
      ImGui::SetClipboardText(text.c_str());
      notify("Scheda copiata negli appunti.");
    }
  }
  void testMarkers() {
    Session probe(root / "evaluation.license");
    check(probe.command(SLX_RS_START));
    probe.step(26000);
    auto state = renderState();
    auto tracking = [&](const SLX_ReelSync_WebTracking &t) {
      return json{{"origin_m", t.material_origin_m},
                  {"upstream_rest_m", t.upstream_rest_m},
                  {"downstream_rest_m", t.downstream_rest_m},
                  {"upstream_route_m", t.upstream_route_m},
                  {"downstream_route_m", t.downstream_route_m}};
    };
    state["web_tracking"] = tracking(probe.webTracking());
    auto next = state;
    probe.step(100);
    next["web_tracking"] = tracking(probe.webTracking());
    check(probe.command(SLX_RS_PAUSE));
    const auto frozen = tracking(probe.webTracking());
    probe.step(100);
    const bool paused = frozen == tracking(probe.webTracking());
    const auto camera_before = renderer.camera_pose();
    renderer.appearance(1, true, true, true, 1);
    renderer.frame();
    auto capture = [&](const json &s, bool marks) {
      renderer.webMarkers(marks, .5f);
      renderer.render(1200, 700, s, false, false, 0);
      return renderer.scenePixels();
    };
    capture(state, true);
    auto a = capture(state, true), b = capture(next, true);
    auto c = capture(state, false), d = capture(next, false);
    auto difference = [](const auto &x, const auto &y) {
      size_t n = 0;
      for (size_t i = 0; i < x.size(); i += 4)
        if (std::abs(int(x[i]) - int(y[i])) + std::abs(int(x[i + 1]) - int(y[i + 1])) +
                std::abs(int(x[i + 2]) - int(y[i + 2])) >
            12)
          ++n;
      return n;
    };
    marker_checks = {{"moving_pixels", difference(a, b)},
                     {"unmarked_changed_pixels", difference(c, d)},
                     {"paused_tracking_frozen", paused},
                     {"material_origin_decreased",
                      next["web_tracking"]["origin_m"] < state["web_tracking"]["origin_m"]}};
    renderer.camera_pose(camera_before);
    renderer.webMarkers(true, .5f);
  }
  void testSupports(const std::filesystem::path &out) {
    auto state = renderState();
    const auto saved = renderer.camera_pose();
    renderer.appearance(1, true, true, true, 1);
    std::vector<unsigned char> first;
    json pose;
    for (int i = 0; i < 4; ++i) {
      pose = {{"center", {-1.9, .64, 0.}},
              {"yaw", .35 + .7 * i},
              {"pitch", .06},
              {"radius", 2.6},
              {"hfov", 1.02}};
      renderer.camera_pose(pose, true);
      renderer.render(1000, 700, state, false, false, 0);
      renderer.saveScene(out / ("support-" + std::to_string(i) + ".png"));
      if (i == 0)
        first = renderer.scenePixels();
    }
    pose["yaw"] = .35;
    renderer.camera_pose(pose, true);
    renderer.appearance(1, true, true, false, 1);
    renderer.render(1000, 700, state, false, false, 0);
    renderer.saveScene(out / "support-no-shadow.png");
    renderer.appearance(1, true, true, true, 1);
    renderer.render(1000, 700, state, false, false, 0);
    support_checks = {{"views", 4},
                      {"camera_roundtrip_identical", first == renderer.scenePixels()}};
    renderer.camera_pose(saved, true);
  }
  void testInstruments(const std::filesystem::path &out) {
    Session probe(root / "evaluation.license");
    check(probe.command(SLX_RS_START));
    probe.step(12000);
    auto first = renderState(&probe);
    const double force = probe.mechanicalReadings().pneumatic_force_n;
    check(probe.command(SLX_RS_INJECT_FAULT, .65, SLX_RS_AIR_LOSS));
    probe.step(600);
    check(probe.command(SLX_RS_PAUSE));
    auto second = renderState(&probe);
    const double reduced = probe.mechanicalReadings().pneumatic_force_n;
    const auto saved = renderer.camera_pose();
    renderer.appearance(1, false, true, true, 1);
    renderer.focus(5);
    const auto hmi_pose = renderer.camera_pose();
    renderer.camera_pose(hmi_pose, true);
    renderer.render(1000, 700, first, false, false, 0);
    auto a = renderer.scenePixels();
    renderer.saveScene(out / "hmi-live.png");
    renderer.render(1000, 700, second, false, false, 0);
    auto b = renderer.scenePixels();
    renderer.saveScene(out / "hmi-fault.png");
    renderer.camera_pose({{"center", {1.95, 1.16, -1.092}},
                          {"yaw", 3.141592653589793},
                          {"pitch", 0.},
                          {"radius", .65},
                          {"hfov", .3}},
                         true);
    renderer.render(700, 700, first, false, false, 0);
    auto c = renderer.scenePixels();
    renderer.saveScene(out / "force-live.png");
    renderer.render(700, 700, second, false, false, 0);
    auto d = renderer.scenePixels();
    renderer.saveScene(out / "force-fault.png");
    auto different = [](const auto &x, const auto &y) {
      size_t n = 0;
      for (size_t i = 0; i < x.size(); i += 4)
        if (x[i] != y[i] || x[i + 1] != y[i + 1] || x[i + 2] != y[i + 2])
          ++n;
      return n;
    };
    probe.step(100);
    renderer.render(700, 700, renderState(&probe), false, false, 0);
    instrument_checks = {{"hmi_changed_pixels", different(a, b)},
                         {"gauge_changed_pixels", different(c, d)},
                         {"pneumatic_force_ratio", reduced / force},
                         {"pneumatic_expected_ratio", .65 + .35 * std::exp(-3.)},
                         {"paused_image_identical", d == renderer.scenePixels()}};
    std::ofstream sample(out / "telemetry.json");
    sample << probe.telemetry().dump(2);
    renderer.camera_pose(saved, true);
  }
  void testAlarmHighlight(const std::filesystem::path &out) {
    const auto camera_saved = renderer.camera_pose();
    renderer.appearance(style, true, true, shadows, light);
    renderer.frame();
    json checks = json::array();
    for (const auto &f : faultCatalog) {
      Session probe(root / "evaluation.license");
      check(probe.command(SLX_RS_START));
      probe.step(3000);
      check(probe.command(SLX_RS_INJECT_FAULT, f.initial, f.id));
      probe.step(500);
      auto state = renderState(&probe);
      auto healthy = state;
      healthy["alarm_status"] = json::object();
      renderer.render(1200, 700, healthy, false, false, 0);
      const auto before = renderer.scenePixels();
      renderer.render(1200, 700, state, false, false, 0);
      const auto after = renderer.scenePixels();
      size_t changed = 0;
      for (size_t i = 0; i < before.size(); i += 4)
        if (before[i] != after[i] || before[i + 1] != after[i + 1] || before[i + 2] != after[i + 2])
          ++changed;
      const int draws = renderer.diagnostics().at("attention_draws");
      if (draws == 0 || changed < 20)
        throw std::runtime_error("Componente allarme non evidenziato nel 3D: " +
                                 std::string(f.name));
      renderer.saveScene(out / ("alarm-" + std::to_string(f.id) + ".png"));
      checks.push_back({{"fault", f.id},
                        {"name", f.name},
                        {"affected_draws", draws},
                        {"changed_pixels", changed}});
    }
    alarm_checks = {{"cases", checks.size()}, {"all_causes_marked", true}, {"results", checks}};
    renderer.appearance(style, hide_guards, hide_hall, shadows, light);
    renderer.camera_pose(camera_saved, true);
  }
  void smokeFrame(int frame) {
    presentation = false;
    clip_play = false;
    if (frame < 4) {
      page = 0;
      if (frame == 1) {
        hide_hall = true;
        hide_guards = true;
        renderer.frame();
      }
      if (frame == 2) {
        auto before = renderer.camera_pose();
        renderer.pan(25, -12);
        renderer.zoom(1);
        auto changed = renderer.camera_pose();
        renderer.camera_pose(before);
        camera_smoke = changed != before && renderer.camera_pose() == before;
      }
      if (frame == 3) {
        selection = "rs_rewinder_root";
        renderer.frame(selection);
      }
    } else if (frame < 6)
      page = 1;
    else if (frame < 8)
      page = 2;
    else if (frame < 10)
      page = 3;
    else if (frame < 12)
      page = 4;
    else if (frame < 14)
      page = 5;
    else if (frame < 16)
      page = frame == 14 ? 7 : 6;
    else {
      page = 0;
      request_about = frame == 16;
    }
    if (frame == 14)
      approval_qualification = {
          {"control_requests",
           json::array({{{"id", "qualification"}, {"name", "Dashboard Raspberry"}}})}};
    if (frame == 16)
      approval_qualification = json::object();
    if (frame == 1) {
      presentation = true;
      clip = 25;
    }
  }
};
} // namespace
int main(int argc, char **argv) {
  GLFWwindow *window = nullptr;
  bool smoke = argc > 1 && std::string(argv[1]) == "--smoke";
  try {
#ifdef _WIN32
    struct InstanceGuard {
      HANDLE handle = nullptr;
      ~InstanceGuard() {
        if (handle)
          CloseHandle(handle);
      }
    } instance;
    if (!smoke) {
      instance.handle = CreateMutexW(nullptr, FALSE, L"Local\\SiliconLogiX.ReelSync.Desktop");
      if (!instance.handle)
        throw std::runtime_error("Impossibile aprire la sessione dell'applicazione.");
      if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(nullptr, L"SLX ReelSync | SiliconLogiX")) {
          ShowWindow(existing, SW_RESTORE);
          SetForegroundWindow(existing);
        }
        return 0;
      }
    }
#endif
    auto root = applicationRoot(argv[0]);
    auto out = smoke ? (argc > 2 ? std::filesystem::path(argv[2]) : root / "smoke")
                     : std::filesystem::path();
    if (!glfwInit())
      throw std::runtime_error("Impossibile inizializzare la grafica desktop");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    if (smoke)
      glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    int width = 1600, height = 1000;
    float smoke_scale = 1;
    if (smoke)
      for (int i = 3; i < argc; ++i) {
        if (i + 1 >= argc)
          throw std::runtime_error("Parametro smoke incompleto.");
        const std::string option = argv[i];
        const std::string value = argv[++i];
        if (option == "--width")
          width = std::stoi(value);
        else if (option == "--height")
          height = std::stoi(value);
        else if (option == "--dpi")
          smoke_scale = std::stof(value);
        else
          throw std::runtime_error("Parametro smoke non valido.");
      }
    if (width < 1050 || width > 3840 || height < 680 || height > 2160 ||
        !std::isfinite(smoke_scale) || smoke_scale < .75f || smoke_scale > 2)
      throw std::runtime_error("Dimensione della finestra non valida.");
    if (!smoke) {
      if (auto *monitor = glfwGetPrimaryMonitor()) {
        int x = 0, y = 0, w = 0, h = 0;
        glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
        width = std::min(width, int(w * .94));
        height = std::min(height, int(h * .93));
      }
    }
    window = glfwCreateWindow(width, height, "SLX ReelSync | SiliconLogiX", nullptr, nullptr);
    if (!window)
      throw std::runtime_error("Driver OpenGL 3.3 richiesto");
    float system_scale = 1, system_y = 1;
    glfwGetWindowContentScale(window, &system_scale, &system_y);
    const float dpi_scale = smoke ? smoke_scale : std::clamp(system_scale, .75f, 2.f);
    glfwSetWindowSizeLimits(window, int(1050 * dpi_scale), int(680 * dpi_scale), GLFW_DONT_CARE,
                            GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(smoke ? 0 : 1);
    slx::desktop::gl::load();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    u::initialize(dpi_scale);
    if (smoke) {
      const auto before = ImGui::GetStyle();
      u::setScale(1.5f);
      u::setScale(1);
      u::setScale(dpi_scale);
      const auto after = ImGui::GetStyle();
      u::chromeChecks["dpi_roundtrip"] = before.WindowPadding.x == after.WindowPadding.x &&
                                         before.WindowPadding.y == after.WindowPadding.y &&
                                         before.ItemSpacing.x == after.ItemSpacing.x &&
                                         before.ItemSpacing.y == after.ItemSpacing.y;
    }
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    {
      Workbench app(root, window, smoke, out);
      if (smoke) {
        std::filesystem::create_directories(out);
        app.setupSmoke();
        app.testMarkers();
        app.testSupports(out);
        app.testInstruments(out);
        app.testAlarmHighlight(out);
      }
      auto last = std::chrono::steady_clock::now();
      std::vector<double> timings;
      int frame = 0;
      while (!glfwWindowShouldClose(window)) {
        auto now = std::chrono::steady_clock::now();
        double wall = std::chrono::duration<double>(now - last).count();
        last = now;
        glfwPollEvents();
        if (!smoke) {
          float sx = 1, sy = 1;
          glfwGetWindowContentScale(window, &sx, &sy);
          u::setScale(sx);
        }
        if (!smoke && glfwWindowShouldClose(window)) {
          try {
            app.checkpoint();
          } catch (const std::exception &e) {
#ifdef _WIN32
            const int answer =
                MessageBoxW(glfwGetWin32Window(window),
                            L"Non e stato possibile salvare l'ultima sessione. Chiudere comunque?",
                            L"SLX ReelSync", MB_YESNO | MB_ICONWARNING);
            if (answer != IDYES) {
              glfwSetWindowShouldClose(window, GLFW_FALSE);
              app.notify(e.what(), true);
              continue;
            }
#endif
          }
          break;
        }
        if (smoke)
          app.smokeFrame(frame);
        try {
          app.tick(wall);
        } catch (const std::exception &e) {
          auto message = std::string(e.what());
          try {
            app.session.command(SLX_RS_PAUSE);
          } catch (...) {
          }
          app.notify(message, true);
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        app.draw();
        ImGui::Render();
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        slx::desktop::gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
        slx::desktop::gl::Viewport(0, 0, w, h);
        slx::desktop::gl::ClearColor(.95f, .96f, .98f, 1);
        slx::desktop::gl::Clear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (smoke) {
          app.renderer.screenshot(out / ("frame-" + std::to_string(frame) + ".png"), w, h);
          if (frame == 17) {
            auto s = app.session.snapshot();
            std::ofstream f(out / "graphics.json");
            f << json({{"renderer", app.renderer.diagnostics()},
                       {"core_time_s", s.sim_s},
                       {"presentation_did_not_step_core", s.sim_s == 13.0},
                       {"camera_controls_exercised", app.camera_smoke},
                       {"logo_aspect", app.logo.aspect},
                       {"frames", frame + 1},
                       {"frame_wall_s", timings},
                       {"chart_ranges", u::chartChecks},
                       {"encoder_scale_test", s.measured_speed_m_s / s.speed_m_s},
                       {"about_visible", app.about_visible},
                       {"remote_approval_visible_any_page", app.approval_visible_any_page},
                       {"material_markers", app.marker_checks},
                       {"support_views", app.support_checks},
                       {"instruments", app.instrument_checks},
                       {"alarm_annunciation", app.alarm_checks},
                       {"viewport", {{"width", w}, {"height", h}, {"dpi_scale", u::scale()}}},
                       {"controls", u::chromeChecks}})
                     .dump(2);
            break;
          }
        }
        glfwSwapBuffers(window);
        timings.push_back(wall);
        ++frame;
      }
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "SLX ReelSync: " << e.what() << '\n';
#ifdef _WIN32
    if (!smoke)
      MessageBoxA(nullptr, e.what(), "SLX ReelSync", MB_OK | MB_ICONERROR);
#endif
    if (window)
      glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }
}
