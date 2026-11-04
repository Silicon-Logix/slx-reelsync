// SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#pragma once
#include "renderer.hpp"
#include <functional>
#include <imgui.h>
#include <implot.h>
#include <initializer_list>
namespace slx::reelsync::ui {
struct Fonts {
  ImFont *body = nullptr;
  ImFont *bold = nullptr;
  ImFont *title = nullptr;
  ImFont *small = nullptr;
};
extern Fonts fonts;
ImVec4 rgb(unsigned hex, float alpha = 1);
void initialize(float dpi_scale = 1);
void setScale(float);
float scale();
extern json chromeChecks;
void heading(const char *, const char *);
void image(const slx::desktop::ImageInfo &, float width);
bool button(const char *, bool enabled = true, bool emphasized = false, ImVec2 size = {0, 0});
bool modeSelector(bool &presentation, bool enabled);
bool infoButton();
bool toggle(const char *label, bool *value);
void nextInline(float width);
void card(const char *id, const char *label, const char *value, const char *detail, unsigned accent,
          float width);
bool nav(const char *text, int icon, bool selected, float width, float height = 43);
void badge(const char *, unsigned color);
struct Series {
  const char *name;
  std::function<double(const SLX_ReelSync_Snapshot &)> value;
  unsigned color;
};
struct ChartView {
  bool follow = true;
  bool full_history = true;
  double window_s = 30;
  bool show_events = true;
};
extern json chartChecks;
void chart(Session &, const char *id, const char *title, const char *unit,
           std::initializer_list<Series>, const ChartView &, float height = 235,
           double minimum_span = .1);
void metricTable(const json &);
void alarms(uint32_t active, uint32_t latched);
bool about(const std::filesystem::path &, Session &, slx::desktop::Renderer &,
           const slx::desktop::ImageInfo &, const slx::desktop::ImageInfo &);
void help();
} // namespace slx::reelsync::ui
