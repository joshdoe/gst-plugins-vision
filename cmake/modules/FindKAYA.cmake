# - Try to find KAYA SDK
# Once done this will define
#
#  KAYA_FOUND - system has KAYA SDK
#  KAYA_INCLUDE_DIR - the KAYA SDK include directory
#  KAYA_LIBRARIES - the libraries needed to use KAYA SDK

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2018 outside US, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT KAYA_DIR)
    set (KAYA_DIR "C:/Program Files/KAYA Instruments/Vision Point" CACHE PATH "Directory containing KAYA SDK includes and libraries")
endif ()

find_path (KAYA_INCLUDE_DIR KYFGLib.h
    PATHS
    "${KAYA_DIR}/include"
    DOC "Directory containing KYFGLib.h include file")

find_library (_KYFGLib NAMES KYFGLib
    PATHS
    "${KAYA_DIR}/lib")

set (KAYA_LIBRARIES ${_KYFGLib})

mark_as_advanced (_KYFGLib)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (KAYA  DEFAULT_MSG  KAYA_INCLUDE_DIR KAYA_LIBRARIES)
