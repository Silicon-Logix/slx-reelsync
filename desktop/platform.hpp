// SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#pragma once
#include <filesystem>
#include <optional>
namespace slx::reelsync {
std::filesystem::path userDataRoot();
std::optional<std::filesystem::path> recipeDialog(void *owner, bool save);
void openDirectory(const std::filesystem::path &);
void openWebsite();
void openReference(const char *https_url);
bool openDocument(const std::filesystem::path &);
void atomicWrite(const std::filesystem::path &, const std::string &text);
} // namespace slx::reelsync
