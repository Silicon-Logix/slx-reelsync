// SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#include "ui.hpp"
#include "faults.hpp"
#include "platform.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
namespace slx::reelsync::ui {
Fonts fonts;
json chartChecks = json::object();
json chromeChecks = json::object();
float current_scale = 1;
ImGuiStyle base_style;
float scale() {
  return current_scale;
}
void setScale(float value) {
  value = std::clamp(value, .75f, 2.f);
  if (std::abs(value - current_scale) > .001f) {
    ImGui::GetStyle() = base_style;
    ImGui::GetStyle().ScaleAllSizes(value);
    current_scale = value;
  }
  ImGui::GetStyle().FontScaleDpi = value;
}
void checkBounds(const char *name, ImVec2 lo, ImVec2 hi) {
  auto *v = ImGui::GetMainViewport();
  chromeChecks[name] = lo.x >= v->Pos.x - 1 && lo.y >= v->Pos.y - 1 &&
                       hi.x <= v->Pos.x + v->Size.x + 1 && hi.y <= v->Pos.y + v->Size.y + 1;
}
ImVec4 rgb(unsigned v, float alpha) {
  return {float((v >> 16) & 255) / 255, float((v >> 8) & 255) / 255, float(v & 255) / 255, alpha};
}
void initialize(float dpi_scale) {
  auto &io = ImGui::GetIO();
  auto add = [&](const char *file, float size) {
    return std::filesystem::exists(file) ? io.Fonts->AddFontFromFileTTF(file, size)
                                         : io.Fonts->AddFontDefault();
  };
  fonts.body = add("C:/Windows/Fonts/segoeui.ttf", 17);
  fonts.bold = add("C:/Windows/Fonts/seguisb.ttf", 18);
  fonts.title = add("C:/Windows/Fonts/seguisb.ttf", 27);
  fonts.small = add("C:/Windows/Fonts/segoeui.ttf", 14);
  io.FontDefault = fonts.body;
  auto &s = ImGui::GetStyle();
  s.WindowPadding = {22, 18};
  s.FramePadding = {12, 8};
  s.ItemSpacing = {10, 10};
  s.ItemInnerSpacing = {8, 6};
  s.ScrollbarSize = 12;
  s.WindowRounding = 10;
  s.ChildRounding = 10;
  s.FrameRounding = 6;
  s.PopupRounding = 10;
  s.TabRounding = 5;
  s.GrabRounding = 5;
  s.WindowBorderSize = 1;
  s.ChildBorderSize = 1;
  s.FrameBorderSize = 1;
  s.DisabledAlpha = .72f;
  auto *c = s.Colors;
  ImGui::StyleColorsLight();
  c[ImGuiCol_Text] = rgb(0x192541);
  c[ImGuiCol_TextDisabled] = rgb(0x56647A);
  c[ImGuiCol_WindowBg] = rgb(0xF2F5F9);
  c[ImGuiCol_ChildBg] = rgb(0xFFFFFF);
  c[ImGuiCol_PopupBg] = rgb(0xFFFFFF);
  c[ImGuiCol_Border] = rgb(0xBDCADC);
  c[ImGuiCol_FrameBg] = rgb(0xEAF0F7);
  c[ImGuiCol_FrameBgHovered] = rgb(0xE0E8F5);
  c[ImGuiCol_FrameBgActive] = rgb(0xD6DFF2);
  c[ImGuiCol_Button] = rgb(0xFFFFFF);
  c[ImGuiCol_ButtonHovered] = rgb(0xEEF0FF);
  c[ImGuiCol_ButtonActive] = rgb(0xDCE0FF);
  c[ImGuiCol_Header] = rgb(0xE9EAFE);
  c[ImGuiCol_HeaderHovered] = rgb(0xE1E5FA);
  c[ImGuiCol_HeaderActive] = rgb(0xD2D9FA);
  c[ImGuiCol_CheckMark] = rgb(0x5354E8);
  c[ImGuiCol_SliderGrab] = rgb(0x5354E8);
  c[ImGuiCol_SliderGrabActive] = rgb(0x4344C8);
  c[ImGuiCol_Separator] = rgb(0xDFE6EF);
  c[ImGuiCol_Tab] = rgb(0xFFFFFF);
  c[ImGuiCol_TabHovered] = rgb(0xDCE0FF);
  c[ImGuiCol_TabSelected] = rgb(0xDCE0FF);
  c[ImGuiCol_TabSelectedOverline] = rgb(0x5354E8);
  c[ImGuiCol_TitleBg] = rgb(0xE6ECF5);
  c[ImGuiCol_TitleBgActive] = rgb(0xE6ECF5);
  ImPlot::StyleColorsLight();
  auto &ps = ImPlot::GetStyle();
  ps.Colors[ImPlotCol_FrameBg] = rgb(0xFFFFFF);
  ps.Colors[ImPlotCol_PlotBg] = rgb(0xFFFFFF);
  ps.Colors[ImPlotCol_PlotBorder] = rgb(0xE1E7F0);
  ps.Colors[ImPlotCol_AxisGrid] = rgb(0xDFE6EF, .7f);
  ps.Colors[ImPlotCol_LegendBg] = rgb(0xFFFFFF, .94f);
  ps.PlotPadding = {13, 12};
  ps.LineWeight = 2;
  base_style = s;
  setScale(dpi_scale);
}
void heading(const char *title, const char *subtitle) {
  ImGui::PushFont(fonts.title);
  ImGui::TextUnformatted(title);
  ImGui::PopFont();
  ImGui::TextColored(rgb(0x6B7A8E), "%s", subtitle);
  ImGui::Spacing();
}
void image(const slx::desktop::ImageInfo &i, float width) {
  ImGui::Image((ImTextureID)(intptr_t)i.texture, {width, width / i.aspect}, {i.uv0[0], i.uv0[1]},
               {i.uv1[0], i.uv1[1]});
}
bool button(const char *text, bool enabled, bool emphasis, ImVec2 size) {
  ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.f);
  ImGui::PushStyleColor(ImGuiCol_Border, rgb(enabled ? 0xA7B7D0 : 0xCDD4DF));
  ImGui::PushStyleColor(ImGuiCol_Text, rgb(enabled ? 0x192541 : 0x56647A));
  ImGui::PushStyleColor(ImGuiCol_Button, rgb(enabled ? 0xFFFFFF : 0xE6EAF0));
  ImGui::BeginDisabled(!enabled);
  if (emphasis && enabled) {
    ImGui::PushStyleColor(ImGuiCol_Button, rgb(0x5354E8));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(0x6465F0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(0x4243CD));
    ImGui::PushStyleColor(ImGuiCol_Text, rgb(0xFFFFFF));
  }
  bool clicked = ImGui::Button(text, size);
  if (emphasis && enabled)
    ImGui::PopStyleColor(4);
  ImGui::EndDisabled();
  if (!enabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("Non disponibile nello stato corrente della linea.");
  ImGui::PopStyleColor(3);
  ImGui::PopStyleVar();
  return clicked;
}
bool modeSelector(bool &presentation, bool enabled) {
  const float k = scale();
  bool changed = false;
  ImGui::PushID("SessionMode");
  ImGui::BeginGroup();
  const auto p = ImGui::GetCursorScreenPos();
  auto *draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(p, {p.x + 260 * k, p.y + 34 * k}, ImGui::GetColorU32(rgb(0xE5EAF3)), 9 * k);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
  ImGui::BeginDisabled(!enabled);
  for (int i = 0; i < 2; ++i) {
    if (i)
      ImGui::SameLine();
    const bool selected = presentation == bool(i);
    ImGui::PushStyleColor(ImGuiCol_Button, rgb(selected ? 0x5354E8 : 0xE5EAF3));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(selected ? 0x4546D2 : 0xD5DDF0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(0x4445C9));
    ImGui::PushStyleColor(ImGuiCol_Text, rgb(selected ? 0xFFFFFF : 0x354460));
    if (ImGui::Button(i ? "Presentazione" : "Simulazione", {130 * k, 34 * k}) && !selected) {
      presentation = bool(i);
      changed = true;
    }
    ImGui::PopStyleColor(4);
  }
  ImGui::EndDisabled();
  ImGui::PopStyleVar(3);
  ImGui::EndGroup();
  checkBounds("mode_selector", ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
  if (!enabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("Metti in pausa la simulazione per cambiare modalita.");
  ImGui::PopID();
  return changed;
}
bool infoButton() {
  const float k = scale();
  const auto p = ImGui::GetCursorScreenPos();
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9);
  const bool clicked = button("##Informazioni sul prodotto", true, false, {36 * k, 34 * k});
  ImGui::PopStyleVar();
  auto *draw = ImGui::GetWindowDrawList();
  const auto ink = ImGui::GetColorU32(rgb(0x5354E8));
  const ImVec2 center{p.x + 18 * k, p.y + 17 * k};
  draw->AddCircle(center, 9 * k, ink, 32, 1.5f * k);
  draw->AddCircleFilled({center.x, center.y - 4 * k}, 1.2f * k, ink);
  draw->AddLine({center.x, center.y}, {center.x, center.y + 5 * k}, ink, 1.8f * k);
  checkBounds("info_button", ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Informazioni sul prodotto  /  F12");
  return clicked;
}
void nextInline(float width) {
  const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
  if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <= right)
    ImGui::SameLine();
}
bool toggle(const char *label, bool *value) {
  const float k = scale();
  ImGui::PushID(label);
  const auto p = ImGui::GetCursorScreenPos(), text = ImGui::CalcTextSize(label);
  ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgb(0xEEF0FF));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgb(0xE1E5FB));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
  const bool changed = ImGui::Button("##Toggle", {42 * k + text.x, 26 * k});
  if (changed)
    *value = !*value;
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(3);
  auto *draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled({p.x, p.y + 4 * k}, {p.x + 32 * k, p.y + 22 * k},
                      ImGui::GetColorU32(rgb(*value ? 0x5354E8 : 0x8491A6)), 9 * k);
  draw->AddCircleFilled({p.x + (*value ? 23 : 9) * k, p.y + 13 * k}, 6.5f * k,
                        ImGui::GetColorU32(rgb(0xFFFFFF)));
  draw->AddText({p.x + 42 * k, p.y + (26 * k - text.y) / 2}, ImGui::GetColorU32(ImGuiCol_Text),
                label);
  ImGui::PopID();
  return changed;
}
void card(const char *id, const char *label, const char *value, const char *detail, unsigned accent,
          float width) {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16, 11});
  ImGui::BeginChild(id, {width, 96 * scale()}, ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PushFont(fonts.small);
  ImGui::TextColored(rgb(0x697A91), "%s", label);
  ImGui::PopFont();
  ImGui::PushFont(fonts.title);
  ImGui::TextColored(rgb(accent), "%s", value);
  ImGui::PopFont();
  ImGui::PushFont(fonts.small);
  ImGui::TextDisabled("%s", detail);
  ImGui::PopFont();
  ImGui::EndChild();
  ImGui::PopStyleVar();
}
bool nav(const char *label, int index, bool selected, float width, float height) {
  ImGui::PushID(index);
  auto p = ImGui::GetCursorScreenPos();
  bool clicked = ImGui::InvisibleButton(label, {width, height});
  bool hovered = ImGui::IsItemHovered();
  auto *d = ImGui::GetWindowDrawList();
  if (selected || hovered)
    d->AddRectFilled(p, {p.x + width, p.y + height},
                     ImGui::ColorConvertFloat4ToU32(rgb(selected ? 0x30345B : 0x202E47)), 7);
  ImU32 color = ImGui::ColorConvertFloat4ToU32(rgb(selected ? 0xBEBDFF : 0x9BABC2));
  ImVec2 q{p.x + 16, p.y + height / 2};
  if (index == 0) {
    d->AddRect({q.x - 7, q.y - 6}, {q.x + 7, q.y + 6}, color, 1, 0, 1.6f);
    d->AddLine({q.x - 7, q.y - 6}, {q.x, q.y - 11}, color, 1.6f);
    d->AddLine({q.x, q.y - 11}, {q.x + 7, q.y - 6}, color, 1.6f);
    d->AddLine(q, {q.x, q.y + 6}, color, 1.6f);
  } else if (index == 3) {
    d->AddLine({q.x - 7, q.y - 8}, {q.x - 7, q.y + 7}, color, 1.7f);
    d->AddLine({q.x - 7, q.y + 7}, {q.x + 8, q.y + 7}, color, 1.7f);
    d->AddLine({q.x - 5, q.y + 3}, {q.x, q.y - 4}, color, 1.7f);
    d->AddLine({q.x, q.y - 4}, {q.x + 6, q.y - 1}, color, 1.7f);
  } else if (index == 4) {
    d->AddTriangle({q.x, q.y - 9}, {q.x - 9, q.y + 7}, {q.x + 9, q.y + 7}, color, 1.6f);
    d->AddLine({q.x, q.y - 3}, {q.x, q.y + 1}, color, 2);
    d->AddCircleFilled({q.x, q.y + 4}, 1, color);
  } else if (index == 5) {
    d->AddRect({q.x - 6, q.y - 9}, {q.x + 6, q.y + 9}, color, 2, 0, 1.6f);
    for (int i = 0; i < 3; ++i)
      d->AddLine({q.x - 3, q.y - 4 + i * 4.f}, {q.x + 3, q.y - 4 + i * 4.f}, color, 1.3f);
  } else {
    for (int i = 0; i < 3; ++i) {
      float y = q.y - 6 + i * 6.f;
      d->AddLine({q.x - 8, y}, {q.x + 8, y}, color, 1.4f);
      d->AddCircleFilled({q.x + (i % 2 ? 3.f : -3.f), y}, 2.6f, color);
    }
  }
  d->AddText(fonts.body, 17 * scale(), {p.x + 40 * scale(), p.y + (height - 17 * scale()) / 2},
             ImGui::ColorConvertFloat4ToU32(rgb(selected ? 0xFFFFFF : 0xCFD8E8)), label);
  ImGui::PopID();
  return clicked;
}
void badge(const char *text, unsigned color) {
  ImGui::PushStyleColor(ImGuiCol_Text, rgb(color));
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
}
void chart(Session &session, const char *id, const char *title, const char *unit,
           std::initializer_list<Series> series, const ChartView &view, float height,
           double minimum_span) {
  ImGui::PushID(id);
  if (!ImPlot::BeginPlot(title, {-1, height}, ImPlotFlags_NoMenus)) {
    ImGui::PopID();
    return;
  }
  const auto &h = session.history;
  double end = h.empty() ? 0 : h.back().sim_s,
         begin = view.full_history ? 0 : std::max(0., end - view.window_s);
  double lo = std::numeric_limits<double>::infinity(), hi = -lo;
  std::vector<double> times;
  std::vector<std::vector<double>> values(series.size());
  size_t k = 0;
  for (const auto &s : h)
    if (s.sim_s >= begin) {
      times.push_back(s.sim_s);
      k = 0;
      for (const auto &line : series) {
        double v = line.value(s);
        values[k++].push_back(v);
        if (std::isfinite(v)) {
          lo = std::min(lo, v);
          hi = std::max(hi, v);
        }
      }
    }
  if (!std::isfinite(lo)) {
    lo = 0;
    hi = minimum_span;
  }
  double padding = std::max((hi - lo) * .12, minimum_span * .15);
  if (hi - lo < minimum_span) {
    double middle = (hi + lo) / 2;
    lo = middle - minimum_span / 2;
    hi = middle + minimum_span / 2;
  }
  double axis_lo = lo - padding, axis_hi = hi + padding;
  ImPlot::SetupAxes("Tempo [s]", unit, ImPlotAxisFlags_NoInitialFit, ImPlotAxisFlags_NoInitialFit);
  if (view.follow) {
    ImPlot::SetupAxisLimits(ImAxis_X1, begin,
                            std::max(begin + 1., end) + std::max(.05, (end - begin) * .01),
                            ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, axis_lo, axis_hi, ImPlotCond_Always);
  } else {
    ImPlot::SetupAxisLimits(ImAxis_X1, begin, std::max(begin + 1., end), ImPlotCond_Once);
    ImPlot::SetupAxisLimits(ImAxis_Y1, axis_lo, axis_hi, ImPlotCond_Once);
  }
  ImPlot::SetupLegend(ImPlotLocation_NorthWest,
                      ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
  k = 0;
  for (const auto &line : series) {
    ImPlot::SetNextLineStyle(rgb(line.color), 2);
    ImPlot::PlotLine(line.name, times.data(), values[k].data(), int(times.size()));
    ++k;
  }
  if (view.show_events) {
    std::vector<double> t;
    for (const auto &e : session.events)
      if (e.type == SLX_RS_INJECT_FAULT && e.result == 0 && e.sim_s >= begin)
        t.push_back(e.sim_s);
    if (!t.empty()) {
      ImPlot::SetNextLineStyle(rgb(0xC69235, .7f), 1);
      ImPlot::PlotInfLines("Guasto", t.data(), int(t.size()));
    }
  }
  auto limits = ImPlot::GetPlotLimits();
  chartChecks[id] = {{"data_min", lo},
                     {"data_max", hi},
                     {"axis_min", limits.Y.Min},
                     {"axis_max", limits.Y.Max},
                     {"all_samples_visible", limits.Y.Min <= lo && limits.Y.Max >= hi}};
  ImPlot::EndPlot();
  ImGui::PopID();
}
void metricTable(const json &m) {
  const std::pair<const char *, const char *> fields[] = {
      {"tension_rms_error_n", "Errore RMS tensione [N]"},
      {"tension_max_abs_error_n", "Errore massimo [N]"},
      {"tension_overshoot_n", "Sovraelongazione [N]"},
      {"settling_final_setpoint_s", "Assestamento finale [s]"},
      {"saturated_sample_fraction", "Frazione campioni in saturazione"},
      {"dancer_min_z_m", "Quota minima ballerino [m]"},
      {"dancer_max_z_m", "Quota massima ballerino [m]"},
      {"max_abs_material_balance_error_m", "Errore massimo bilancio [m]"}};
  if (ImGui::BeginTable("metriche", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
    ImGui::TableSetupColumn("Grandezza", ImGuiTableColumnFlags_WidthStretch, 2);
    ImGui::TableSetupColumn("Risultato", ImGuiTableColumnFlags_WidthStretch, 1);
    ImGui::TableHeadersRow();
    for (auto [key, label] : fields)
      if (m.contains(key)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        if (m[key].is_null())
          ImGui::TextDisabled("Non osservato");
        else
          ImGui::Text("%.6g", m[key].get<double>());
      }
    ImGui::EndTable();
  }
}
void alarms(uint32_t active, uint32_t latched) {
  if (!active && !latched) {
    badge("Nessun allarme", 0x16866D);
    return;
  }
  for (size_t i = 0; i < alarmNames.size(); ++i)
    if ((active | latched) & (1u << i)) {
      ImGui::TextColored(rgb(active & (1u << i) ? 0xBE5555 : 0x9C7A37), "%s  /  %s", alarmNames[i],
                         active & (1u << i) ? "ATTIVO" : "IN MEMORIA");
    }
}
std::string textFile(const std::filesystem::path &p) {
  std::ifstream f(p);
  return f ? std::string((std::istreambuf_iterator<char>(f)), {}) : "Documento non disponibile.";
}
bool about(const std::filesystem::path &root, Session &session, slx::desktop::Renderer &renderer,
           const slx::desktop::ImageInfo &logo, const slx::desktop::ImageInfo &icon) {
  const float k = scale();
  const auto viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowSize(
      {std::min(850.f * k, viewport->Size.x - 40), std::min(720.f * k, viewport->Size.y - 40)},
      ImGuiCond_Appearing);
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, {.5f, .5f});
  bool opened = true, visible = false;
  if (ImGui::BeginPopupModal("Informazioni su SLX ReelSync", &opened,
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoScrollWithMouse)) {
    visible = true;
    checkBounds("about", ImGui::GetWindowPos(),
                {ImGui::GetWindowPos().x + ImGui::GetWindowWidth(),
                 ImGui::GetWindowPos().y + ImGui::GetWindowHeight()});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, rgb(0x111C31));
    ImGui::PushStyleColor(ImGuiCol_Text, rgb(0xFFFFFF));
    ImGui::BeginChild("ProductBanner", {0, 140 * k}, ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    image(icon, 68 * k);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushFont(fonts.title);
    ImGui::TextUnformatted("SLX ReelSync");
    ImGui::PopFont();
    ImGui::TextColored(rgb(0xCDD5F0), "SIMULATION LAB  /  %s", SLX_RS_VERSION);
    ImGui::TextUnformatted("Web handling & winding control");
    ImGui::EndGroup();
    ImGui::SameLine(std::max(500.f * k, ImGui::GetWindowWidth() - 190 * k));
    image(logo, 150 * k);
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    static int tab = 0;
    const char *tabs[] = {"Prodotto", "Versioni e SDK", "Licenza", "Componenti"};
    for (int i = 0; i < 4; ++i) {
      if (i)
        ImGui::SameLine();
      if (button(tabs[i], true, tab == i, {(i == 1 ? 175.f : 145.f) * k, 38 * k}))
        tab = i;
    }
    ImGui::BeginChild("AboutBody", {0, -62 * k}, ImGuiChildFlags_AlwaysUseWindowPadding);
    if (tab == 0) {
      ImGui::PushFont(fonts.bold);
      ImGui::TextUnformatted("Dalla ricetta alla diagnosi del processo");
      ImGui::PopFont();
      ImGui::TextWrapped("Il simulatore SiliconLogiX per studiare tensione, velocità, diametri e "
                         "ballerino su una linea di avvolgimento. Prove ripetibili, anomalie "
                         "programmabili e risultati esportabili.");
      ImGui::SeparatorText("SiliconLogiX");
      ImGui::TextUnformatted("Autore: Marco Pezzullo  /  Silicon LogiX");
      ImGui::TextUnformatted("Piattaforma: Windows x64");
      ImGui::TextUnformatted("Esecuzione: locale e offline  /  nessun invio automatico di dati");
      ImGui::TextWrapped("Ricette, viste e prove vengono salvate nel profilo locale dell'utente. "
                         "CSV, eventi e metriche restano disponibili nella cartella Risultati.");
      ImGui::SeparatorText("Un ambiente per l'analisi del processo");
      ImGui::TextWrapped(
          "Configura la linea, confronta le regolazioni e studia la risposta ai guasti. "
          "La guida descrive le funzioni e il campo di applicazione del simulatore.");
      ImGui::Spacing();
      static bool guide_error = false;
      if (button("Guida utente"))
        guide_error = !openDocument(root / "docs/Guida.html");
      ImGui::SameLine();
      if (button("Sito SiliconLogiX", true, true))
        openWebsite();
      if (guide_error)
        ImGui::TextColored(rgb(0xAF303D),
                           "Guida non disponibile. Verifica l'estrazione completa del pacchetto.");
    } else if (tab == 1) {
      ImGui::SeparatorText("Versioni");
      ImGui::Text("Applicazione: %s", SLX_RS_VERSION);
      ImGui::Text("Core: %s  /  SDK: %s  /  ABI: %u", session.build.core_version, SLX_RS_VERSION,
                  SLX_RS_ABI);
      ImGui::Text("Modello: %s", session.build.model_version);
      ImGui::Text("Build: %s", session.build.build_id);
      ImGui::Text("Capability: 0x%llx", (unsigned long long)session.build.capabilities);
      ImGui::SeparatorText("Architettura");
      ImGui::TextWrapped("C++20 / OpenGL 3.3. Core SiliconLogiX con SDK C versionato "
                         "e controllore didattico personalizzabile.");
      if (button("Copia informazioni di sistema", true, true)) {
        json info = {{"product", "SLX ReelSync"},
                     {"host", SLX_RS_VERSION},
                     {"core", session.build.core_version},
                     {"abi", SLX_RS_ABI},
                     {"model", session.build.model_version},
                     {"build", session.build.build_id},
                     {"renderer", renderer.diagnostics()}};
        ImGui::SetClipboardText(info.dump(2).c_str());
      }
    } else if (tab == 2) {
      ImGui::TextWrapped("%s", textFile(root / "LICENSE").c_str());
      ImGui::SeparatorText("SDK");
      ImGui::TextWrapped("%s", textFile(root / "SDK_LICENSE").c_str());
    } else {
      ImGui::TextWrapped("%s", textFile(root / "THIRD_PARTY_NOTICES.md").c_str());
    }
    ImGui::EndChild();
    ImGui::Separator();
    if (button("Chiudi", true, true, {120 * k, 36 * k}) || ImGui::IsKeyPressed(ImGuiKey_Escape))
      ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    ImGui::TextDisabled("Copyright (c) 2026 Marco Pezzullo  /  Silicon LogiX");
    ImGui::EndPopup();
  }
  return visible;
}

void help() {
  ImGui::SetNextWindowSize({700, 560}, ImGuiCond_Appearing);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {.5f, .5f});
  bool open = true;
  if (ImGui::BeginPopupModal("Guida rapida", &open, ImGuiWindowFlags_NoResize)) {
    heading("Dalla ricetta alla diagnosi", "Una prova completa, con dati e cause osservabili.");
    ImGui::TextWrapped(
        "1. Scegli una ricetta in Prova e ricette e applicala a macchina ferma.\n\n2. Avvia la "
        "linea. In Regolazione puoi cambiare velocità e tensione.\n\n3. In Guasti e allarmi "
        "inietta un'anomalia o programmane inizio e durata. Rimuovere la causa non cancella una "
        "fermata memorizzata.\n\n4. Analizza i Grafici, poi esporta CSV, configurazione, eventi e "
        "report da Risultati.");
    ImGui::SeparatorText("Vista 3D");
    ImGui::TextWrapped(
        "Trascinamento sinistro o destro: orbita. Centrale oppure Maiusc + sinistro: pan. Rotella: "
        "zoom. Clic: selezione. Doppio clic / F: inquadra gruppo. Home: macchina completa. Viste "
        "camera e preferiti dalla barra della scena.");
    ImGui::SeparatorText("Comandi rapidi");
    ImGui::TextWrapped("Spazio: avvio / pausa / ripresa analisi. S: arresto controllato. F12: "
                       "About. La pausa congela il tempo e non equivale a una frenatura fisica. "
                       "Presentazione riproduce solo la clip.");
    if (button("Ho capito", true, true) || ImGui::IsKeyPressed(ImGuiKey_Escape))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}
} // namespace slx::reelsync::ui
