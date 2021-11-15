# - Try to find Pleora SDK
# Once done this will define
#
#  Pleora_FOUND - system has Pleora SDK
#  Pleora_INCLUDE_DIR - the Pleora SDK include directory
#  Pleora_LIBRARIES - the Pleora SDK libraries
#  Pleora_LIBRARY_DIR - the Pleora SDK library directory

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2019 outside US, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT Pleora_DIR)
    set (Pleora_DIR $ENV{PUREGEV_ROOT} CACHE PATH "Directory containing Pleora SDK includes and libraries")
endif ()

if (WIN32 AND CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(_LIB_SUFFIX "64")
else ()
    set(_LIB_SUFFIX "")
endif ()

set (_Pleora_PATHS PATHS
  "${Pleora_DIR}"
  "C:/Program Files/Pleora Technologies Inc/eBUS SDK/Includes"
  "C:/Program Files (x86)/Pleora Technologies Inc/eBUS SDK/Includes")

find_path (Pleora_INCLUDE_DIR PvBase.h
    PATHS ${_Pleora_PATHS}
    PATH_SUFFIXES Includes include)
message (STATUS "Found Pleora include dir in ${Pleora_INCLUDE_DIR}")

find_path (Pleora_LIBRARY_DIR NAMES libPvBase.so "PvBase${_LIB_NAME}"
    PATHS ${_Pleora_PATHS}
    PATH_SUFFIXES Libraries lib)

message (STATUS "Found Pleora library in ${Pleora_LIBRARY_DIR}")

find_library (Pleora_LIBRARY_BASE "PvBase${_LIB_SUFFIX}" ${Pleora_LIBRARY_DIR})
find_library (Pleora_LIBRARY_DEVICE "PvDevice${_LIB_SUFFIX}" ${Pleora_LIBRARY_DIR})
find_library (Pleora_LIBRARY_PERSISTENCE "PvPersistence${_LIB_SUFFIX}" ${Pleora_LIBRARY_DIR})
find_library (Pleora_LIBRARY_VIRTUAL_DEVICE "PvVirtualDevice${_LIB_SUFFIX}" ${Pleora_LIBRARY_DIR})

set (Pleora_LIBRARIES ${Pleora_LIBRARY_BASE} ${Pleora_LIBRARY_DEVICE} ${Pleora_LIBRARY_PERSISTENCE} ${Pleora_LIBRARY_VIRTUAL_DEVICE})

if (Pleora_INCLUDE_DIR)
  file(STRINGS "${Pleora_INCLUDE_DIR}/PvVersion.h" _pleora_VERSION_CONTENTS REGEX "#define NVERSION_STRING")
  if ("${_pleora_VERSION_CONTENTS}" MATCHES "#define NVERSION_STRING[ \t]+\"([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)+")
    set(Pleora_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(Pleora_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(Pleora_VERSION_PATCH "${CMAKE_MATCH_3}")
    set(Pleora_VERSION_TWEAK "${CMAKE_MATCH_4}")
    set(Pleora_VERSION_COUNT 4)
    set(Pleora_VERSION_STRING "${Pleora_VERSION_MAJOR}.${Pleora_VERSION_MINOR}.${Pleora_VERSION_PATCH}.${Pleora_VERSION_TWEAK}")
    set(Pleora_VERSION ${Pleora_VERSION_STRING})
    message(STATUS "Found Pleora version: ${Pleora_VERSION_STRING}")
  endif ()
endif ()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (Pleora
  REQUIRED_VARS Pleora_INCLUDE_DIR Pleora_LIBRARY_DIR Pleora_LIBRARIES
  VERSION_VAR Pleora_VERSION_STRING)
