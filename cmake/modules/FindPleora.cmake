# - Try to find Pleora SDK
# Once done this will define
#
#  PLEORA_FOUND - system has Pleora SDK
#  PLEORA_INCLUDE_DIR - the Pleora SDK include directory
#  PLEORA_LIBRARIES_DIR - the Pleora SDK libraries directory

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2016 outside US, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT PLEORA_DIR)
    set (PLEORA_DIR "C:/Program Files (x86)/Pleora Technologies Inc/eBUS SDK" CACHE PATH "Directory containing Pleora SDK includes and libraries")
endif ()

if (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(_LIB_SUFFIX "64")
else ()
    set(_LIB_SUFFIX "")
endif ()

find_path (PLEORA_INCLUDE_DIR PvBase.h
    PATHS
    "${PLEORA_DIR}/Includes"
    DOC "Directory containing Pleora eBUS SDK headers")

find_path (PLEORA_LIBRARIES_DIR PvBase${_LIB_SUFFIX}.lib
    PATHS
    "${PLEORA_DIR}/Libraries"
    DOC "Directory containing Pleora eBUS SDK libraries")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (PLEORA  DEFAULT_MSG  PLEORA_INCLUDE_DIR PLEORA_LIBRARIES_DIR)