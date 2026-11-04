// Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#pragma once
#include <nlohmann/json.hpp>
#include <vector>
namespace slx::desktop {
struct PanelPixels {
  int width, height;
  std::vector<unsigned char> rgba;
};
PanelPixels operatorPanel(const nlohmann::json &telemetry, bool presentation);
PanelPixels forceDial(double force_n, bool presentation);
} // namespace slx::desktop
