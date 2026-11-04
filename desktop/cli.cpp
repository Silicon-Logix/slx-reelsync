// SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#include "educational_controller.h"
#include "session.hpp"
#include "recipes.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace slx::reelsync;
int main(int argc, char **argv) {
  try {
    std::string scenario = "ramp", controller = "reference", out = "results";
    double seconds = 60;
    double length_target = 0;
    bool pause_alarm = false;
    std::string recipe, replay;
    for (int i = 1; i < argc; ++i) {
      std::string a = argv[i];
      if(a=="--list-materials"){std::cout<<materialCatalog().dump(2)<<'\n';return 0;}
      if (a == "--help") {
        std::cout << "SLX ReelSync --scenario "
                     "ramp|setpoint|encoder|bias|friction|break|drive|air|encoder-scale|stuck|lag|"
                     "recovery --controller reference|educational --seconds 60 --output results "
                     "--recipe config.json --length 25 --pause-on-alarm\n";
        return 0;
      }
      if (a == "--pause-on-alarm") {
        pause_alarm = true;
        continue;
      }
      if (i + 1 >= argc)
        throw std::runtime_error("Missing argument value");
      std::string value = argv[++i];
      if (a == "--scenario")
        scenario = value;
      else if (a == "--controller")
        controller = value;
      else if (a == "--seconds")
        seconds = std::stod(value);
      else if (a == "--output")
        out = value;
      else if (a == "--recipe")
        recipe = value;
      else if (a == "--replay")
        replay = value;
      else if (a == "--length")
        length_target = std::stod(value);
      else
        throw std::runtime_error("Unknown argument " + a);
    }
    if (!std::isfinite(seconds) || seconds <= 0 || seconds > 600)
      throw std::runtime_error("seconds must be 0..600");
    if (controller != "reference" && controller != "educational")
      throw std::runtime_error("Unknown controller");
    Session session(applicationRoot(argv[0], false) / "evaluation.license");
    auto c = session.config;
    json recording;
    if (!replay.empty()) {
      std::ifstream f(replay);
      recording = json::parse(f);
      if (recording.at("schema") != "slx.reelsync.trial.v1" ||
          recording.at("model_version") != session.build.model_version ||
          recording.at("build_id") != session.build.build_id)
        throw std::runtime_error("Incompatible core/model recording");
      if (!recording.at("controller_source_sha256").is_null() &&
          recording.at("controller_source_sha256") != SLX_ReelSync_EducationalSourceHash())
        throw std::runtime_error("Educational source differs from recorded trial");
    }
    if (!recipe.empty() || !replay.empty()) {
      json r;
      if (!replay.empty())
        r = recording.at("configuration");
      else {
        std::ifstream f(recipe);
        r = json::parse(f);
        if (r.contains("configuration"))
          r = r.at("configuration");
      }
#define CFG(field)                                                                                 \
  if (r.contains(#field))                                                                          \
  c.field = r.at(#field).get<decltype(c.field)>()
      CFG(dt_s);
      CFG(width_m);
      CFG(thickness_m);
      CFG(density_kg_m3);
      CFG(young_pa);
      CFG(unwind_radius_m);
      CFG(rewind_radius_m);
      CFG(tension_n);
      CFG(speed_m_s);
      CFG(acceleration_m_s2);
      CFG(torque_limit_nm);
      CFG(dancer_mass_kg);
      CFG(sensor_noise_n);
      CFG(sensor_bias_n);
      CFG(sensor_delay_s);
      CFG(seed);
#undef CFG
      if (!replay.empty())
        controller = r.at("controller") == "reference-1" ? "reference" : "educational";
    }
    c.controller = controller == "reference" ? 1 : 2;
    session.configure(c);
    auto start = std::chrono::steady_clock::now();
    if (replay.empty()) {
      session.setLengthTarget(length_target);
      session.setPauseOnAlarm(pause_alarm);
      runScenario(session, scenario, seconds);
    } else {
      scenario = recording.at("scenario");
      session.restore(recording);
    }
    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    session.exportTrial(out, scenario);
    std::cout << json({{"metrics", session.metrics()},
                       {"final_state", stateName(session.snapshot().state)},
                       {"sim_s", session.snapshot().sim_s},
                       {"wall_s", elapsed},
                       {"output", out}})
                     .dump(2)
              << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "SLX ReelSync: " << e.what() << '\n';
    return 1;
  }
}
