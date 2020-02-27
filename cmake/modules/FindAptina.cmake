# - Try to find Aptina SDK
# Once done this will define
#
#  APTINA_FOUND - system has Aptina SDK
#  APTINA_INCLUDE_DIR - the Aptina SDK include directory
#  APTINA_LIBRARIES - the libraries needed to use Aptina SDK

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2017 outside US, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT APTINA_DIR)
    set (APTINA_DIR "C:/Aptina Imaging" CACHE PATH "Directory containing Aptina SDK includes and libraries")
endif ()

if (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(_LIB_NAME "apbase")
else ()
    set(_LIB_NAME "apbase")
endif ()

find_path (APTINA_INCLUDE_DIR apbase.h
    PATHS
    "${APTINA_DIR}/include"
    DOC "Directory containing Aptina include files")

find_library (APTINA_LIBRARIES NAMES ${_LIB_NAME}
    PATHS
    "${APTINA_DIR}/lib")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (APTINA  DEFAULT_MSG  APTINA_INCLUDE_DIR APTINA_LIBRARIES)