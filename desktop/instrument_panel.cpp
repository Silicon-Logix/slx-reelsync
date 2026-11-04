// Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#include "instrument_panel.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
namespace slx::desktop {
namespace {
std::string number(double v, int decimals) {
  std::ostringstream s;
  s << std::fixed << std::setprecision(decimals) << v;
  return s.str();
}
#ifdef _WIN32
struct Canvas {
  HDC dc = nullptr;
  HBITMAP bitmap = nullptr;
  HGDIOBJ old = nullptr;
  unsigned char *pixels = nullptr;
  int w, h;
  Canvas(int width, int height) : w(width), h(height) {
    dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap =
        CreateDIBSection(dc, &info, DIB_RGB_COLORS, reinterpret_cast<void **>(&pixels), nullptr, 0);
    if (!dc || !bitmap) {
      if (bitmap)
        DeleteObject(bitmap);
      if (dc)
        DeleteDC(dc);
      throw std::runtime_error("Impossibile creare il pannello strumenti.");
    }
    old = SelectObject(dc, bitmap);
    SetBkMode(dc, TRANSPARENT);
    fill(0, 0, w, h, 0x111C31);
  }
  ~Canvas() {
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
  }
  static COLORREF color(unsigned c) {
    return RGB((c >> 16) & 255, (c >> 8) & 255, c & 255);
  }
  void fill(int x, int y, int width, int height, unsigned c) {
    RECT r{x, y, x + width, y + height};
    auto b = CreateSolidBrush(color(c));
    FillRect(dc, &r, b);
    DeleteObject(b);
  }
  void text(int x, int y, const std::string &s, int size, unsigned c = 0xF2F5FC,
            bool bold = false) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring wide(n, L' ');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), wide.data(), n);
    HFONT font = CreateFontW(-size, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    auto previous = SelectObject(dc, font);
    SetTextColor(dc, color(c));
    TextOutW(dc, x, y, wide.data(), n);
    SelectObject(dc, previous);
    DeleteObject(font);
  }
  void line(int x, int y, int xx, int yy, unsigned c, int width = 2) {
    auto pen = CreatePen(PS_SOLID, width, color(c));
    auto previous = SelectObject(dc, pen);
    MoveToEx(dc, x, y, nullptr);
    LineTo(dc, xx, yy);
    SelectObject(dc, previous);
    DeleteObject(pen);
  }
  void circle(int x, int y, int r, unsigned c) {
    auto b = CreateSolidBrush(color(c));
    auto p = CreatePen(PS_SOLID, 1, color(c));
    auto ob = SelectObject(dc, b), op = SelectObject(dc, p);
    Ellipse(dc, x - r, y - r, x + r, y + r);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
  }
  PanelPixels result(bool round = false) {
    GdiFlush();
    PanelPixels out{w, h, std::vector<unsigned char>(size_t(w) * h * 4)};
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        size_t i = (size_t(y) * w + x) * 4;
        out.rgba[i] = pixels[i + 2];
        out.rgba[i + 1] = pixels[i + 1];
        out.rgba[i + 2] = pixels[i];
        out.rgba[i + 3] = round && std::hypot(x - w * .5, y - h * .5) > w * .49 ? 0 : 255;
      }
    return out;
  }
};
#endif
} // namespace
PanelPixels operatorPanel(const nlohmann::json &t, bool presentation) {
#ifdef _WIN32
  Canvas c(960, 600);
  const bool connected = !t.empty() && !presentation;
  const auto p = t.value("process", nlohmann::json::object()),
             s = t.value("sensors", nlohmann::json::object()),
             a = t.value("alarms", nlohmann::json::object());
  c.text(35, 24, "ReelSync", 45, 0xFFFFFF, true);
  c.text(35, 79, "LINEA DI AVVOLGIMENTO", 18, 0xA5B5D0);
  const bool alarm = connected && a.value("active", 0u);
  const bool degraded = connected && (s.value("quality", 0u) != 1 || a.value("fault", 0u) != 0);
  c.fill(35, 123, 890, 66, alarm ? 0x842F40 : degraded ? 0x71512E : 0x19463F);
  if (connected)
    c.text(630, 38,
           s.value("quality", 0u) == 0   ? "SEGNALI NON VALIDI"
           : s.value("quality", 0u) == 2 ? "SEGNALI ALTERATI"
           : degraded                    ? "ANOMALIA INIETTATA"
                                         : "SEGNALI NOMINALI",
           19, degraded ? 0xFFCF8B : 0xA5D6C7, true);
  const std::string label = presentation ? "PRESENTAZIONE"
                            : connected  ? t.at("state").at("label").get<std::string>()
                                         : "IN ATTESA";
  c.text(56, 133, label, 32, 0xFFFFFF, true);
  const char *labels[] = {"VELOCITA [m/s]", "TENSIONE [N]", "BALLERINO [m]"};
  const char *keys[] = {"speed_m_s", "tension_r_n", "dancer_position_m"};
  for (int i = 0; i < 3; ++i) {
    int x = 35 + i * 302;
    c.fill(x, 215, 286, 133, 0x1E304D);
    c.text(x + 18, 231, labels[i], 18, 0xA5B5D0);
    c.text(x + 18, 264, connected ? number(p.value(keys[i], 0.), i == 1 ? 1 : 3) : "--", 46,
           0xF4F7FF, true);
  }
  c.text(35, 375,
         connected ? "DIAMETRI  U " + number(p.value("unwind_diameter_m", 0.), 3) + " m   /   R " +
                         number(p.value("rewind_diameter_m", 0.), 3) + " m"
                   : "DIAMETRI  --",
         23);
  c.text(35, 416,
         connected && s.value("quality", 0u) != 0
             ? "ENCODER " + number(s.value("encoder_m_s", 0.), 3) + " m/s   |   CELLA " +
                   number(s.value("tension_n", 0.), 1) + " N"
             : "ENCODER  --   |   CELLA  --",
         23, 0xCAD4EB);
  c.text(35, 457,
         connected ? "NIP " + number(p.value("nip_travel_m", 0.), 2) + " m   |   ATTUATORE " +
                         number(p.value("pneumatic_force_n", 0.), 1) + " N"
                   : "NIP  --",
         23, 0xCAD4EB);
  c.fill(35, 511, 890, 2, 0x354664);
  c.text(35, 535, "SiliconLogiX", 23, 0xA8A6FF, true);
  c.text(640, 539, connected ? number(t.value("sim_s", 0.), 3) + " s" : "Sequenza della linea", 21,
         0xCAD4EB);
  return c.result();
#else
  return {960, 600, std::vector<unsigned char>(960 * 600 * 4, 32)};
#endif
}
PanelPixels forceDial(double force, bool presentation) {
#ifdef _WIN32
  Canvas c(256, 256);
  c.fill(0, 0, 256, 256, 0xF7F8FC);
  c.circle(128, 128, 123, 0x253651);
  c.circle(128, 128, 118, 0xF7F8FC);
  const double pi = 3.14159265358979323846;
  for (int i = 0; i <= 10; ++i) {
    double angle = (225 - i * 27) * pi / 180;
    int r = i % 5 == 0 ? 91 : 98;
    c.line(128 + int(r * std::cos(angle)), 128 - int(r * std::sin(angle)),
           128 + int(108 * std::cos(angle)), 128 - int(108 * std::sin(angle)), 0x273650,
           i % 5 == 0 ? 3 : 2);
  }
  c.text(29, 174, "-400", 17, 0x56647A);
  c.text(183, 174, "+400", 17, 0x56647A);
  c.text(69, 200, "FORZA [N]", 19, 0x263651, true);
  if (!presentation) {
    double angle = (225 - 270 * std::clamp((force+400.) / 800., 0., 1.)) * pi / 180;
    c.line(128, 128, 128 + int(81 * std::cos(angle)), 128 - int(81 * std::sin(angle)), 0xC13750, 5);
    c.circle(128, 128, 7, 0x273650);
    c.text(89, 156, number(force, 1), 24, 0x192541, true);
  } else
    c.text(106, 157, "--", 24, 0x56647A);
  return c.result(true);
#else
  return {256, 256, std::vector<unsigned char>(256 * 256 * 4, 255)};
#endif
}
} // namespace slx::desktop
