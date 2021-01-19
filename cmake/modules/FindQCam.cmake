# - Try to find QCam SDK
# Once done this will define
#
#  QCAM_FOUND - system has QCam SDK
#  QCAM_INCLUDE_DIR - the QCam SDK include directory
#  QCAM_LIBRARIES - the libraries needed to use QCam SDK

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2021 outside US, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT QCAM_DIR)
    set (QCAM_DIR "C:/Program Files/QImaging/SDK" CACHE PATH "Directory containing QCam SDK includes and libraries")
endif ()

if (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(_LIB_NAME "QCamDriverx64")
else ()
    set(_LIB_NAME "QCamDriver")
endif ()

find_path (QCAM_INCLUDE_DIR QCamApi.h
    PATHS
    "${QCAM_DIR}/Headers"
    DOC "Directory containing QCam API include files")

find_library (QCAM_LIBRARIES NAMES ${_LIB_NAME}
    PATHS
    "${QCAM_DIR}/libs/AMD64" "${QCAM_DIR}/libs/i386")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (QCAM  DEFAULT_MSG  QCAM_INCLUDE_DIR QCAM_LIBRARIES)