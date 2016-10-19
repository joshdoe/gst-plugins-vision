# - Try to find Bitflow SDK
# Once done this will define
#
#  BITFLOW_FOUND - system has Bitflow SDK
#  BITFLOW_INCLUDE_DIR - the Bitflow SDK include directory
#  BITFLOW_LIBRARIES - the libraries needed to use Bitflow SDK

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2016 outside US, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT BITFLOW_DIR)
    set (BITFLOW_DIR "C:/BitFlow SDK 6.20" CACHE PATH "Directory containing Bitflow SDK includes and libraries")
endif ()

if (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(_LIB_PATH "${BITFLOW_DIR}/Lib64")
else ()
    set(_LIB_PATH "${BITFLOW_DIR}/Lib32")
endif ()

find_path (BITFLOW_INCLUDE_DIR BiApi.h
    PATHS
    "${BITFLOW_DIR}/Include"
    DOC "Directory containing BiApi.h include file")

find_library (_BidLib NAMES bid
    PATHS
    "${_LIB_PATH}")

find_library (_BfdLib NAMES bfd
    PATHS
    "${_LIB_PATH}")

set (BITFLOW_LIBRARIES ${_BidLib} ${_BfdLib})

mark_as_advanced (_BidLib _BfdLib)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (BITFLOW  DEFAULT_MSG  BITFLOW_INCLUDE_DIR BITFLOW_LIBRARIES)