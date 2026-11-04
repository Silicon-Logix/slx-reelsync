/* SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
 * SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
 * Editable proportional example. No private model implementation. */
#include "educational_controller.h"
const char* SLX_ReelSync_EducationalSourceHash(void) { return SLX_RS_EDUCATIONAL_HASH; }
void SLX_RS_CALL SLX_ReelSync_EducationalControl(const SLX_ReelSync_ControlInput* in,
                                               SLX_ReelSync_ControlOutput* out, void* user) {
  (void)user;
  out->unwind_speed_trim_m_s=0;
  out->rewind_speed_trim_m_s=.001*(in->tension_setpoint_n-in->measured_tension_n);
}
