// SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <cstring>
#include <filesystem>
#include <fstream>
#include <slx/reelsync.h>
#include <string>
#include <windows.h>
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR arguments, int) {
  bool check_only = std::wstring(arguments) == L"--check";
  auto fail = [&](const wchar_t *message, int code) {
    if (!check_only)
      MessageBoxW(nullptr, message, L"SLX ReelSync", MB_OK | MB_ICONERROR);
    return code;
  };
  try {
    wchar_t executable[32768];
    DWORD n = GetModuleFileNameW(nullptr, executable, 32768);
    if (!n || n >= 32768)
      return fail(L"Percorso dell'applicazione non valido.", 1);
    auto root = std::filesystem::path(executable).parent_path();
    auto dll = root / L"bin/SLX_ReelSync_Core.dll";
    if (!std::filesystem::exists(dll))
      return fail(
          L"Core SiliconLogiX mancante. Estrarre l'intero pacchetto prima di avviare SLX ReelSync.",
          2);
    HMODULE core = LoadLibraryExW(dll.c_str(), nullptr,
                                  LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!core)
      return fail(L"Impossibile caricare il core Windows x64. Il pacchetto potrebbe essere "
                  L"incompleto o incompatibile.",
                  3);
    using InfoFn = SLX_ReelSync_Result(SLX_RS_CALL *)(SLX_ReelSync_BuildInfo *);
    auto get_info = reinterpret_cast<InfoFn>(GetProcAddress(core, "SLX_ReelSync_GetBuildInfo"));
    SLX_ReelSync_BuildInfo info{};
    info.struct_size = sizeof(info);
    info.abi_version = SLX_RS_ABI;
    bool compatible = get_info && get_info(&info) == 0 &&
                      !std::strcmp(info.product, "SLX ReelSync") &&
                      !std::strcmp(info.core_version, SLX_RS_VERSION);
    FreeLibrary(core);
    if (!compatible)
      return fail(L"Versione del core incompatibile con SLX ReelSync "
                  L"1.5.0"
                  L". Estrarre nuovamente il pacchetto completo.",
                  4);
    std::ifstream f(root / L"evaluation.license", std::ios::binary);
    std::string license((std::istreambuf_iterator<char>(f)), {});
    if (license != SLX_RS_EVALUATION_TEXT)
      return fail(L"Licenza di valutazione offline mancante o non valida.", 5);
    auto app = root / L"bin/SLX-ReelSync.exe";
    if (!std::filesystem::exists(app) ||
        !std::filesystem::exists(root / L"assets/scene/SLX_ReelSync_Runtime.glb") ||
        !std::filesystem::exists(root / L"assets/branding/reelsync.png"))
      return fail(L"Applicazione o risorse mancanti. Estrarre il pacchetto completo.", 6);
    if (check_only)
      return 0;
    std::wstring command = L"\"" + app.wstring() + L"\" " + arguments;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(app.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        root.c_str(), &startup, &process))
      return fail(L"Avvio dell'applicazione non riuscito.", 7);
    CloseHandle(process.hThread);
    bool smoke = std::wstring(arguments).starts_with(L"--smoke");
    DWORD result = 0;
    if (smoke) {
      WaitForSingleObject(process.hProcess, INFINITE);
      GetExitCodeProcess(process.hProcess, &result);
    }
    CloseHandle(process.hProcess);
    return int(result);
  } catch (...) {
    return fail(L"Pacchetto non leggibile o errore di avvio.", 8);
  }
}
