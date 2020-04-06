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
    set (PYLON_DIR "C:/Program Files/Basler/pylon 5" CACHE PATH "Directory containing Pylon SDK includes and libraries")
endif ()

find_path (PYLON_INCLUDE_DIR pylonc/PylonC.h
    PATHS
    "${PYLON_DIR}/Development/include"
    DOC "Directory containing KYFGLib.h include file")

# TODO: support multiple SDK versions
find_library (_PylonCLib NAMES PylonC_MD_VC120
    PATHS
    "${PYLON_DIR}/Development/lib/x64")

set (PYLON_LIBRARIES ${_PylonCLib})

mark_as_advanced (_PylonCLib)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (PYLON  DEFAULT_MSG  PYLON_INCLUDE_DIR PYLON_LIBRARIES)
