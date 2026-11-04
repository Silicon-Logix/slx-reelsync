// Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#pragma once
#include "session.hpp"
namespace slx::reelsync {
// Transport-independent parsing only. It does not open a socket or submit commands.
SLX_ReelSync_Command decodeCommand(const json &, const std::string &expected_run_id);
} // namespace slx::reelsync
