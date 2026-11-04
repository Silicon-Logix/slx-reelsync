/* SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
 * SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0 */
#pragma once
#include <slx/reelsync.h>
#ifdef __cplusplus
extern "C" {
#endif
void SLX_RS_CALL SLX_ReelSync_EducationalControl(const SLX_ReelSync_ControlInput*, SLX_ReelSync_ControlOutput*, void*);
const char* SLX_ReelSync_EducationalSourceHash(void);
#ifdef __cplusplus
}
#endif
