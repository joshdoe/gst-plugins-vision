# - Try to find Pylon SDK
# Once done this will define
#
#  PYLON_FOUND - system has Pylon SDK
#  PYLON_INCLUDE_DIR - the Pylon SDK include directory
#  PYLON_LIBRARIES - the libraries needed to use Pylon SDK

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT PYLON_DIR)
  if (WIN32)
    set (_PYLON_DIR "C:/Program Files/Basler/pylon 5")
  else ()
    set (_PYLON_DIR "/opt/pylon5")
  endif ()
  set (PYLON_DIR ${_PYLON_DIR} CACHE PATH "Directory containing Pylon SDK includes and libraries")
endif ()

find_path (PYLON_INCLUDE_DIR pylonc/PylonC.h
    PATHS
    "${PYLON_DIR}/Development/include"
    "${PYLON_DIR}/include"
    DOC "Directory containing PylonC.h include file")

# TODO: support multiple SDK versions
find_library (_PylonCLib NAMES PylonC_MD_VC120 pylonc
    PATHS
    "${PYLON_DIR}/Development/lib/x64"
    "${PYLON_DIR}/lib64"
    "${PYLON_DIR}/lib"
    )

set (PYLON_LIBRARIES ${_PylonCLib})

mark_as_advanced (_PylonCLib)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (PYLON  DEFAULT_MSG  PYLON_INCLUDE_DIR PYLON_LIBRARIES)
