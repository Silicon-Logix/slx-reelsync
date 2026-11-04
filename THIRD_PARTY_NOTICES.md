<!-- SPDX-FileCopyrightText: 2026 Marco Pezzullo (Silicon LogiX)
SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0 -->
# Third-party components and assets

Original SLX copyright: 2026 Marco Pezzullo (Silicon LogiX).
Third-party copyright and license texts below are preserved independently.

| Component | Fixed revision | License / included text |
| --- | --- | --- |
| GLFW | 3.4 | zlib, licenses/glfw-LICENSE.txt |
| Dear ImGui | 1.92.4-docking | MIT, licenses/imgui-LICENSE.txt |
| ImPlot | 0.17 | MIT, licenses/implot-LICENSE.txt |
| cgltf | 1.15 | MIT, licenses/cgltf-LICENSE.txt |
| stb_image / stb_image_write | f0569113c93ad095470c54bf34a17b36646bbbb5 | MIT/public domain choice, licenses/stb-LICENSE.txt |
| nlohmann/json | 3.12.0 | MIT, licenses/json-LICENSE.MIT |
| LLVM-MinGW runtime | toolchain 20261020, Clang 23.1.4 | Apache-2.0 with LLVM exception and bundled runtime notices, licenses/llvm-runtime-LICENSE.txt |
| MinGW-w64 runtime / winpthreads | bundled with toolchain 20261020 | Original mixed permissive runtime notices, licenses/COPYING.MinGW-w64-runtime.txt and licenses/COPYING.winpthreads.txt |
| ProggyClean embedded font in Dear ImGui | upstream imgui_draw.cpp | MIT, Tristan Grimmer, licenses/ProggyClean-NOTICE.txt |
| Japanese glyph-range data in Dear ImGui source | upstream imgui_draw.cpp | CC BY 4.0, licenses/imgui-glyph-ranges-NOTICE.txt |
| DejaVu glyph use in supplied assets | supplied asset revision 1 | licenses/DejaVu_font_notice.txt and complete upstream licenses/DejaVu-LICENSE.txt |
| Original 3D assets / logo | supplied ReelSync asset revision 1 | ASSET_LICENSE; original delivery notice preserved in licenses/assets-original-NOTICE.txt |

OpenGL and Windows system components are supplied by the operating system and
driver. Segoe UI is read from the installed Windows font directory and is not
redistributed. Font fallback uses the font supplied by Dear ImGui.
The optional TLS/WebSocket transport service uses Go 1.27.3 (BSD-3-Clause,
licenses/Go-LICENSE.txt) and Gorilla WebSocket 1.5.3 (BSD-2-Clause,
licenses/gorilla-websocket-LICENSE.txt). It is supplied as a separate executable.
The Raspberry HMI uses LVGL 9.5 and licensed Montserrat fonts; their notices and
the Linux component licences are included in the embedded image.

Dependency source origins and archive checksums are in scripts/dependencies.lock.json;
the release SBOM records the files actually shipped. Dependency library sources
are provided in third_party/ for rebuilding. No SLX restriction reduces the
independent permissions of those third-party licenses.

The GitHub review tree contains the dependency source files needed to rebuild
this application, with their original headers and license notices. Unused vendor
examples, test fixtures, standalone font files and prebuilt example binaries
are excluded. The ProggyClean font embedded in Dear ImGui remains covered by
its own MIT notice. The preserved Debian DejaVu notice also describes Debian
packaging files; those packaging files are not distributed by ReelSync.

API references consulted for integration: [GLFW window/context guide](https://www.glfw.org/docs/3.4/window_guide.html),
[cgltf v1.15](https://github.com/jkuhlmann/cgltf/tree/v1.15),
[Dear ImGui v1.92.4-docking](https://github.com/ocornut/imgui/tree/v1.92.4-docking).
Versions remain pinned to the locally tested sources; no latest-version claim.
