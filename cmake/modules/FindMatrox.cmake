# - Try to find Matrox MIL SDK
# Once done this will define
#
#  MATROX_FOUND - system has Matrox MIL SDK
#  MATROX_INCLUDE_DIR - the Matrox MIL SDK include directory
#  MATROX_LIBRARIES - the libraries needed to use Matrox MIL SDK

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2017 outside US, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT MATROX_DIR)
    set (MATROX_DIR "C:/Program Files/Matrox Imaging/Mil" CACHE PATH "Directory containing Matrox MIL SDK includes and libraries")
endif ()

find_path (MATROX_INCLUDE_DIR Mil.h
    PATHS
    "${MATROX_DIR}/Include"
    DOC "Directory containing Matrox MIL include files")

find_library (_MATROX_LIB NAMES Mil.lib
    PATHS
    "${MATROX_DIR}/LIB")


set (MATROX_LIBRARIES ${_MATROX_LIB})

mark_as_advanced (_MATROX_LIB)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (MATROX  DEFAULT_MSG  MATROX_INCLUDE_DIR MATROX_LIBRARIES)