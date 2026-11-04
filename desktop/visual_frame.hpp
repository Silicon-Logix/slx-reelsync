// Copyright (c) 2026 Marco Pezzullo / Silicon LogiX.
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
// Public asset layout and SDK output only; no plant/controller implementation.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <nlohmann/json.hpp>
#include <slx/reelsync.h>
#include <vector>
namespace slx::reelsync {
inline nlohmann::json visualFrame(const SLX_ReelSync_Snapshot &s,
                                  const SLX_ReelSync_WebTracking &material,
                                  const std::vector<SLX_ReelSync_PathPoint> &path) {
  struct Roller {
    const char *id;
    double x, z, r;
  };
  const std::array<Roller, 9> layout = {{{"unwinder", -4.3, 1.55, s.unwind_radius_m},
                                         {"entry_idler", -2.9, 2.25, .1},
                                         {"loadcell_roller", -1.9, 2.05, .12},
                                         {"drive_roller", -.9, 2.25, .14},
                                         {"dancer_guide_in", .2, 2.35, .1},
                                         {"dancer_roller", 1, s.dancer_z_m, .13},
                                         {"dancer_guide_out", 1.8, 2.35, .1},
                                         {"exit_idler", 2.7, 2.15, .12},
                                         {"rewinder", 4.3, 1.55, s.rewind_radius_m}}};
  nlohmann::json rollers = nlohmann::json::array(), points = nlohmann::json::array();
  for (size_t i = 0; i < layout.size(); ++i) {
    const auto &r = layout[i];
    rollers.push_back(
        {{"id", r.id}, {"center_m", {r.x, r.z}}, {"radius_m", r.r}, {"phase_rad", s.phase_rad[i]}});
  }
  // Bounded subpixel simplification of the SDK polyline. Keep its material
  // distance coordinate, including the SDK's documented visual clearance.
  std::vector<bool> keep(path.size());
  auto simplify = [&](auto &&self, size_t first, size_t last) -> void {
    keep[first] = keep[last] = true;
    if (last <= first + 1)
      return;
    const auto &a = path[first], &b = path[last];
    double maximum = 0;
    size_t index = first;
    const double dx = b.x_m - a.x_m, dz = b.y_m - a.y_m, denom = dx * dx + dz * dz;
    for (size_t i = first + 1; i < last; ++i) {
      const auto &p = path[i];
      const double t =
          denom > 0 ? std::clamp(((p.x_m - a.x_m) * dx + (p.y_m - a.y_m) * dz) / denom, 0., 1.)
                    : 0.;
      double d = std::hypot(p.x_m - a.x_m - t * dx, p.y_m - a.y_m - t * dz);
      if (d > maximum) {
        maximum = d;
        index = i;
      }
    }
    if (maximum > .0008) {
      self(self, first, index);
      self(self, index, last);
    }
  };
  if (!path.empty())
    simplify(simplify, 0, path.size() - 1);
  for (size_t i = 0; i < path.size(); ++i)
    if (keep[i])
      points.push_back({path[i].x_m, path[i].y_m, path[i].distance_m});
  return {{"schema", "slx.reelsync.visual.v1"},
          {"rollers", rollers},
          {"path_m", points},
          {"material",
           {{"origin_m", material.material_origin_m},
            {"upstream_rest_m", material.upstream_rest_m},
            {"downstream_rest_m", material.downstream_rest_m},
            {"upstream_route_m", material.upstream_route_m},
            {"downstream_route_m", material.downstream_route_m},
            {"connected", bool(material.connected)}}}};
}
} // namespace slx::reelsync
