// SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
// Adapted from the Silicon LogiX SolarPilot renderer by the same author.
#pragma once
#include "session.hpp"
#include <array>
#include <memory>
struct GLFWwindow;
namespace slx::desktop {
using json = nlohmann::json;
struct RendererImpl;
struct ImageInfo {
  unsigned texture = 0;
  float aspect = 1;
  std::array<float, 2> uv0{0, 0}, uv1{1, 1};
};
class Renderer {
  std::unique_ptr<RendererImpl> p;

public:
  explicit Renderer(const std::filesystem::path &asset_root);
  ~Renderer();
  unsigned render(int width, int height, const json &state, bool cutaway, bool presentation,
                  double clip_s, const json &unused = json::object());
  unsigned image_file(const std::filesystem::path &);
  ImageInfo image_info(const std::filesystem::path &);
  void window_icon(GLFWwindow *, const std::filesystem::path &);
  void orbit(float dx, float dy);
  void zoom(float amount);
  void focus(int preset);
  void exposure(float);
  void pan(float dx, float dy);
  void update(double real_dt_s);
  void frame(const std::string &semantic = "");
  json camera_pose() const;
  void camera_pose(const json &, bool immediate = false);
  void appearance(int material_style, bool guards_hidden, bool hall_hidden, bool shadows,
                  float key_light = 1);
  void webMarkers(bool enabled, float pitch_m = .5f);
  std::vector<unsigned char> scenePixels() const;
  void saveScene(const std::filesystem::path &) const;
  std::string pick(float x, float y) const;
  json diagnostics() const;
  void screenshot(const std::filesystem::path &, int width, int height);
};
} // namespace slx::desktop
