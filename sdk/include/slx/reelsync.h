/* SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
 * SPDX-License-Identifier: LicenseRef-SLX-ReelSync-SDK-Evaluation-1.0
 * ABI 1: SI units, cdecl, natural 8-byte alignment; no STL or exceptions. */
#ifndef SLX_REELSYNC_H
#define SLX_REELSYNC_H
#include <stdint.h>
#if defined(_WIN32)
#define SLX_RS_CALL __cdecl
#if defined(SLX_RS_BUILD_CORE)
#define SLX_RS_API __declspec(dllexport)
#else
#define SLX_RS_API __declspec(dllimport)
#endif
#else
#define SLX_RS_CALL
#define SLX_RS_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define SLX_RS_ABI 1u
#define SLX_RS_VERSION "1.5.0"
#define SLX_RS_MAX_PATH_POINTS 512u
#define SLX_RS_EVALUATION_TEXT "SLX ReelSync | SiliconLogiX | offline technical evaluation | ABI 1 | 2026\n"
typedef struct SLX_ReelSync_Context* SLX_ReelSync_Handle;
typedef int32_t SLX_ReelSync_Result;
enum { SLX_RS_OK=0, SLX_RS_INVALID=1, SLX_RS_ABI_ERROR=2, SLX_RS_STATE_ERROR=3,
       SLX_RS_LICENSE_ERROR=4, SLX_RS_CAPABILITY_ERROR=5, SLX_RS_INTERNAL=6,
       SLX_RS_STALE_COMMAND=7, SLX_RS_BUFFER_ERROR=8 };
enum { SLX_RS_CONFIGURING=0, SLX_RS_READY=1, SLX_RS_RUNNING=2, SLX_RS_STOPPING=3,
       SLX_RS_STOPPED=4, SLX_RS_PAUSED=5, SLX_RS_FAULT=6, SLX_RS_MAINTENANCE=7 };
enum { SLX_RS_START=1, SLX_RS_STOP=2, SLX_RS_PAUSE=3, SLX_RS_RESUME=4,
       SLX_RS_RESET=5, SLX_RS_SET_SPEED=6, SLX_RS_SET_TENSION=7,
       SLX_RS_ENTER_MAINTENANCE=8, SLX_RS_EXIT_MAINTENANCE=9,
       SLX_RS_INJECT_FAULT=10, SLX_RS_CLEAR_FAULT=11, SLX_RS_SINGLE_STEP=12,
       SLX_RS_REMOVE_FAULT=13, SLX_RS_ACKNOWLEDGE=14 };
enum { SLX_RS_NO_FAULT=0, SLX_RS_ENCODER_LOSS=1, SLX_RS_TENSION_BIAS=2,
       SLX_RS_FRICTION=3, SLX_RS_WEB_BREAK=4, SLX_RS_DRIVE_DERATING=5,
       SLX_RS_AIR_LOSS=6, SLX_RS_ENCODER_SCALE=7, SLX_RS_TENSION_STUCK=8,
       SLX_RS_SENSOR_LAG=9 };
enum { SLX_RS_ALARM_ENCODER=1, SLX_RS_ALARM_SPEED=2, SLX_RS_ALARM_TENSION=4,
       SLX_RS_ALARM_DANCER=8, SLX_RS_ALARM_SATURATION=16, SLX_RS_ALARM_ENVELOPE=32,
       SLX_RS_ALARM_BREAK=64, SLX_RS_ALARM_CONTROLLER=128, SLX_RS_ALARM_MODEL_BOUNDARY=256 };
enum { SLX_RS_REFERENCE=1, SLX_RS_EDUCATIONAL=2 };
enum { SLX_RS_DIAG_CAN_ACKNOWLEDGE=1 };
enum { SLX_RS_CAP_SIMULATION=1, SLX_RS_CAP_EDUCATIONAL=2, SLX_RS_CAP_FAULTS=4,
       SLX_RS_CAP_LIVE_PATH=8 };
typedef struct {
  uint32_t struct_size, abi_version;
  double dt_s, width_m, thickness_m, density_kg_m3, young_pa;
  double unwind_radius_m, rewind_radius_m, tension_n, speed_m_s, acceleration_m_s2;
  double torque_limit_nm, dancer_mass_kg, sensor_noise_n, sensor_bias_n, sensor_delay_s;
  uint64_t seed;
  uint32_t controller, reserved;
} SLX_ReelSync_Config;
typedef struct {
  uint32_t struct_size, abi_version;
  uint64_t id;
  double expires_at_sim_s; /* 0 = no expiry; positive deadline in simulation time */
  uint32_t type, argument;
  double value;
} SLX_ReelSync_Command;
typedef struct {
  uint32_t struct_size, abi_version;
  double sim_s, dt_s, speed_m_s, measured_speed_m_s, tension_n, measured_tension_n;
  double tension_setpoint_n, unwind_radius_m, rewind_radius_m, dancer_z_m;
} SLX_ReelSync_ControlInput;
typedef struct {
  uint32_t struct_size, abi_version;
  double unwind_speed_trim_m_s, rewind_speed_trim_m_s; /* core clamps to +/-0.3 */
} SLX_ReelSync_ControlOutput;
/* Synchronous on Step's thread, caller-owned user data. Never throw/reenter;
 * pointers valid for callback only. Native module is trusted local code. */
typedef void (SLX_RS_CALL *SLX_ReelSync_Controller)(const SLX_ReelSync_ControlInput*,
                                                SLX_ReelSync_ControlOutput*, void*);
typedef struct {
  uint32_t struct_size, abi_version;
  uint64_t sequence, last_command_id;
  uint32_t state, fault, sensor_quality, saturated;
  double sim_s, speed_m_s, speed_setpoint_m_s, measured_speed_m_s;
  double tension_u_n, tension_r_n, tension_setpoint_n, measured_tension_n;
  double unwind_radius_m, rewind_radius_m, unwind_mass_kg, rewind_mass_kg;
  double unwind_inertia_kg_m2, rewind_inertia_kg_m2;
  double unwind_torque_nm, rewind_torque_nm, nip_torque_nm;
  double dancer_z_m, dancer_velocity_m_s;
  double coil_u_length_m, coil_r_length_m, free_u_rest_m, free_r_rest_m;
  double free_geometric_m, material_balance_error_m, transported_m;
  double phase_rad[9]; /* native +Y, route order; GLB axis mapping in scene contract */
} SLX_ReelSync_Snapshot;
typedef struct { double x_m, y_m, z_m, distance_m; } SLX_ReelSync_PathPoint;
typedef struct {
  uint32_t struct_size, abi_version;
  char vendor[32], product[32], core_version[16], model_version[24], build_id[48];
  uint64_t capabilities;
} SLX_ReelSync_BuildInfo;
typedef struct {
  uint32_t struct_size, abi_version;
  uint64_t sequence, command_id;
  double sim_s;
  uint32_t type;
  int32_t result;
  uint32_t argument, reserved;
  double value;
  char message[120];
} SLX_ReelSync_Event;
typedef struct {
  uint32_t struct_size, abi_version;
  uint32_t active_alarms, latched_alarms, paused_state;
  union { uint32_t command_flags; uint32_t reserved; }; /* ABI 1 slot; flags since 1.5.0 */
  double fault_severity, fault_elapsed_s, encoder_error_m_s, torque_utilization;
} SLX_ReelSync_Diagnostics;
/* Material coordinates in rest metres, increasing along the free web route.
 * Route lengths include the scene's 0.4 mm visual clearance. No buffer ownership. */
typedef struct {
  uint32_t struct_size, abi_version;
  double material_origin_m, upstream_rest_m, downstream_rest_m;
  double upstream_route_m, downstream_route_m;
  uint32_t connected, reserved;
} SLX_ReelSync_WebTracking;
typedef struct {
  uint32_t struct_size, abi_version;
  double pneumatic_force_n, pneumatic_available_fraction; /* signed actuator load, +downward; gravity separate */
  double free_slack_u_m, free_slack_r_m;
  double unwind_power_w, rewind_power_w, nip_power_w;
} SLX_ReelSync_MechanicalReadings;
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_DefaultConfig(SLX_ReelSync_Config*);
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_Create(uint32_t abi, const char* license_text,
    uint32_t license_bytes, SLX_ReelSync_Handle* out_handle);
SLX_RS_API void SLX_RS_CALL SLX_ReelSync_Destroy(SLX_ReelSync_Handle);
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_Configure(SLX_ReelSync_Handle, const SLX_ReelSync_Config*);
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_SubmitCommand(SLX_ReelSync_Handle, const SLX_ReelSync_Command*);
/* Exactly one configured integration interval; paused state leaves time frozen. */
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_Step(SLX_ReelSync_Handle);
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_GetSnapshot(SLX_ReelSync_Handle, SLX_ReelSync_Snapshot*);
/* Writes glTF Y-up points; no additional coordinate conversion in renderer.
 * Caller owns buffer; insufficient capacity returns required_count without writes. */
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_GetPath(SLX_ReelSync_Handle,
    SLX_ReelSync_PathPoint* points, uint32_t capacity, uint32_t* required_count);
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_GetCapabilities(SLX_ReelSync_Handle, uint64_t*);
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_GetBuildInfo(SLX_ReelSync_BuildInfo*);
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_GetDiagnostics(SLX_ReelSync_Handle, SLX_ReelSync_Diagnostics*);
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_GetWebTracking(SLX_ReelSync_Handle, SLX_ReelSync_WebTracking*);
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_GetMechanicalReadings(SLX_ReelSync_Handle, SLX_ReelSync_MechanicalReadings*);
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_SetController(SLX_ReelSync_Handle, SLX_ReelSync_Controller, void*);
/* Ring of latest 256 events, non-destructive; after_sequence=0 reads retained history. */
SLX_RS_API SLX_ReelSync_Result SLX_RS_CALL SLX_ReelSync_ReadEvents(SLX_ReelSync_Handle,
    uint64_t after_sequence, SLX_ReelSync_Event*, uint32_t capacity, uint32_t* count);
#ifdef __cplusplus
}
#endif
#endif
