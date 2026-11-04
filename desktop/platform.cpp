// SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#include "platform.hpp"
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// clang-format off
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
// clang-format on
#endif
namespace slx::reelsync {
void atomicWrite(const std::filesystem::path &path, const std::string &text) {
  std::filesystem::create_directories(path.parent_path());
  auto pending = path;
  pending += L".pending";
  {
    std::ofstream file(pending, std::ios::binary | std::ios::trunc);
    file.write(text.data(), std::streamsize(text.size()));
    file.flush();
    if (!file)
      throw std::runtime_error("Impossibile salvare la sessione.");
  }
#ifdef _WIN32
  if (!MoveFileExW(pending.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    throw std::runtime_error("Impossibile completare il salvataggio della sessione.");
#else
  std::filesystem::rename(pending, path);
#endif
}
std::filesystem::path userDataRoot() {
#ifdef _WIN32
  PWSTR p = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) {
    std::filesystem::path root(p);
    CoTaskMemFree(p);
    root /= "SiliconLogiX/ReelSync";
    std::filesystem::create_directories(root);
    return root;
  }
#else
  if (auto p = std::getenv("XDG_DATA_HOME")) {
    auto root = std::filesystem::path(p) / "SiliconLogiX/ReelSync";
    std::filesystem::create_directories(root);
    return root;
  }
  if (auto p = std::getenv("HOME")) {
    auto root = std::filesystem::path(p) / ".local/share/SiliconLogiX/ReelSync";
    std::filesystem::create_directories(root);
    return root;
  }
#endif
  throw std::runtime_error("Cartella dati utente non disponibile");
}
std::optional<std::filesystem::path> recipeDialog(void *owner, bool save) {
#ifdef _WIN32
  wchar_t file[32768] = L"";
  auto initial = userDataRoot();
  std::wstring dir = initial.wstring();
  OPENFILENAMEW d{};
  d.lStructSize = sizeof(d);
  d.hwndOwner = static_cast<HWND>(owner);
  d.lpstrFile = file;
  d.nMaxFile = 32768;
  d.lpstrFilter = L"Ricetta ReelSync (*.json)\0*.json\0\0";
  d.lpstrDefExt = L"json";
  d.lpstrInitialDir = dir.c_str();
  d.lpstrTitle = save ? L"Salva ricetta SLX ReelSync" : L"Apri ricetta SLX ReelSync";
  d.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
            (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
  if (save ? GetSaveFileNameW(&d) : GetOpenFileNameW(&d))
    return std::filesystem::path(file);
  if (CommDlgExtendedError())
    throw std::runtime_error("Finestra di selezione file non disponibile");
#else
  (void)owner;
  (void)save;
#endif
  return std::nullopt;
}
void openDirectory(const std::filesystem::path &p) {
  std::filesystem::create_directories(p);
#ifdef _WIN32
  if (reinterpret_cast<intptr_t>(
          ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
    throw std::runtime_error("Impossibile aprire la cartella");
#endif
}
void openWebsite() {
#ifdef _WIN32
  ShellExecuteW(nullptr, L"open", L"https://www.siliconlogix.it", nullptr, nullptr, SW_SHOWNORMAL);
#endif
}
bool openDocument(const std::filesystem::path &path) {
  if (!std::filesystem::is_regular_file(path))
    return false;
#ifdef _WIN32
  return reinterpret_cast<intptr_t>(
             ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
#else
  return false;
#endif
}
void openReference(const char *url) {
  std::string value(url);
  if (value.rfind("https://", 0) != 0)
    throw std::runtime_error("Indirizzo di riferimento non valido");
#ifdef _WIN32
  std::wstring wide(value.begin(), value.end());
  if (reinterpret_cast<intptr_t>(
          ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
    throw std::runtime_error("Impossibile aprire il riferimento");
#endif
}
} // namespace slx::reelsync
