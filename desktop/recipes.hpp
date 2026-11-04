// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
// Copyright (c) 2026 Marco Pezzullo / Silicon LogiX.
// Public material data and SDK configuration; no plant implementation.
#pragma once
#include <array>
#include <cmath>
#include <nlohmann/json.hpp>
#include <slx/reelsync.h>
#include <stdexcept>
namespace slx::reelsync {
struct MaterialRecipe {
  const char *key, *name, *note, *source;
  double width_m, thickness_m, young_pa, density_kg_m3;
};
inline constexpr std::array<MaterialRecipe, 6> materialRecipes{
    {{"elastic-1mm", "Elastico 1,0 mm", "Profilo elastico di riferimento", "", .8, .001, 6e7, 950},
     {"elastic-0p5mm", "Elastico 0,5 mm", "Profilo elastico di riferimento", "", .8, .0005, 1.2e8,
      950},
     {"PET-25um", "PET 25 um", "Film poliestere, modulo longitudinale tipico",
      "https://www.grafixplastics.com/materials-plastic-film-plastic-sheets/commodity-plastic-film/"
      "polyester-mylar-duralar-film/duralar-polyester-film-sheet-products/"
      "clear-duralar-polyester-film/",
      .4, .000025, 4.9e9, 1390},
     {"LDPE-100um", "PE-LD 100 um", "Polietilene a bassa densita, valori tipici Lupolen 2420F",
      "https://www.lyondellbasell.com/en/polymers/p/Lupolen-2420F/"
      "61e46490-ed79-4c71-bd7a-35125886bb3a",
      .8, .0001, 2.6e8, 923},
     {"BOPP-20um", "BOPP 20 um", "Polipropilene orientato, modulo film 0-1%, riferimento HP522H",
      "https://hongrunplastics.com/public/uploads/images/20250609/"
      "LyondellBasell%20PP%20Moplen%20HP522H.pdf",
      .8, .00002, 2.9e9, 900},
     {"PA6-40um", "PA6 40 um condizionato",
      "Film cast condizionato, Ultramid B36 LN 01; densita media dell'intervallo pubblicato",
      "https://chemicals.basf.com/dam/jcr:794caec6-5ccb-4345-8cc5-26f39840adca/"
      "BASF_Ultramid_Properties_and_Extrusion_Processing_Guide.pdf",
      .8, .00004, 7e8, 1135}}};
inline void applyMaterialRecipe(SLX_ReelSync_Config &c, int id) {
  if (id < 0 || id >= int(materialRecipes.size()))
    throw std::runtime_error("Ricetta non disponibile");
  const auto &m = materialRecipes[size_t(id)];
  c.width_m = m.width_m;
  c.thickness_m = m.thickness_m;
  c.young_pa = m.young_pa;
  c.density_kg_m3 = m.density_kg_m3;
}
inline int materialRecipeId(const SLX_ReelSync_Config &c) {
  auto same = [](double a, double b) { return std::abs(a - b) <= 1e-9 * std::abs(b); };
  for (size_t i = 0; i < materialRecipes.size(); ++i) {
    const auto &m = materialRecipes[i];
    if (same(c.width_m, m.width_m) && same(c.thickness_m, m.thickness_m) &&
        same(c.young_pa, m.young_pa) && same(c.density_kg_m3, m.density_kg_m3))
      return int(i);
  }
  return -1;
}
inline nlohmann::json materialCatalog() {
  auto out = nlohmann::json::array();
  for (size_t i = 0; i < materialRecipes.size(); ++i) {
    const auto &m = materialRecipes[i];
    out.push_back({{"id", i},
                   {"key", m.key},
                   {"name", m.name},
                   {"note", m.note},
                   {"source", m.source},
                   {"width_m", m.width_m},
                   {"thickness_m", m.thickness_m},
                   {"young_pa", m.young_pa},
                   {"density_kg_m3", m.density_kg_m3}});
  }
  return out;
}
inline nlohmann::json materialInfo(const SLX_ReelSync_Config &c) {
  const int id = materialRecipeId(c);
  return id < 0 ? nlohmann::json{{"id", -1}, {"key", "custom"}, {"name", "Personalizzato"}}
                : materialCatalog()[size_t(id)];
}
} // namespace slx::reelsync
