# - Try to find IDS uEye SDK
# Once done this will define
#
#  IDSUEYE_FOUND - system has IDS uEye SDK
#  IDSUEYE_INCLUDE_DIR - the IDS uEye SDK include directory
#  IDSUEYE_LIBRARIES - the libraries needed to use IDS uEye SDK

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2017 outside US, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT IDSUEYE_DIR)
    set (IDSUEYE_DIR "C:/Program Files/IDS/uEye/Develop" CACHE PATH "Directory containing IDS uEye SDK includes and libraries")
endif ()

if (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(_LIB_NAME "uEye_api_64")
else ()
    set(_LIB_NAME "uEye_api")
endif ()

find_path (IDSUEYE_INCLUDE_DIR uEye.h
    PATHS
    "${IDSUEYE_DIR}/include"
    DOC "Directory containing IDS uEye include files")

find_library (_uEyeLib NAMES ${_LIB_NAME}
    PATHS
    "${IDSUEYE_DIR}/Lib")


set (IDSUEYE_LIBRARIES ${_uEyeLib})

mark_as_advanced (_uEyeLib)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (IDSUEYE  DEFAULT_MSG  IDSUEYE_INCLUDE_DIR IDSUEYE_LIBRARIES)