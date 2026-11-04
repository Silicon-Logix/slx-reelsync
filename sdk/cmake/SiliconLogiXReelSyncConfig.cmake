# SLX ReelSync — Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).
# SPDX-License-Identifier: LicenseRef-SLX-ReelSync-SDK-Evaluation-1.0
get_filename_component(_slx_sdk "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_library(SiliconLogiX::ReelSync SHARED IMPORTED GLOBAL)
if(WIN32)
  set_target_properties(SiliconLogiX::ReelSync PROPERTIES IMPORTED_LOCATION "${_slx_sdk}/bin/SLX_ReelSync_Core.dll" IMPORTED_IMPLIB "${_slx_sdk}/lib/libSLX_ReelSync_Core.dll.a")
else()
  set_target_properties(SiliconLogiX::ReelSync PROPERTIES IMPORTED_LOCATION "${_slx_sdk}/bin/SLX_ReelSync_Core.so")
endif()
set_target_properties(SiliconLogiX::ReelSync PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_slx_sdk}/include")
