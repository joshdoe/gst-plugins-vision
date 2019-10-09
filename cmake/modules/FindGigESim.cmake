# - Try to find A&B Soft GigESim
# Once done this will define
#
#  GIGESIM_FOUND - system has A&B Soft GigESim
#  GIGESIM_INCLUDE_DIR - the A&B Soft GigESim include directory
#  GIGESIM_LIBRARIES - the libraries needed to use the A&B Soft GigESim

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT GIGESIM_DIR)
	set (GIGESIM_DIR "C:/Program Files/GigESim" CACHE PATH "Directory containing A&B Soft GigESim")
endif (NOT GIGESIM_DIR)

find_path (GIGESIM_INCLUDE_DIR GigeSimSDK.h
    PATHS
    "${GIGESIM_DIR}/Include"
    DOC "Directory containing GigESim include file")

if (CMAKE_SIZEOF_VOID_P MATCHES "8")
find_library (GIGESIM_LIBRARIES NAMES gigesimsdk64.lib
    PATHS
    "${GIGESIM_DIR}/Lib"
    DOC "GIGESIM library to link with")
else ()
find_library (GIGESIM_LIBRARIES NAMES gigesimsdk
    PATHS
    "${GIGESIM_DIR}/Lib"
    DOC "GIGESIM library to link with")
endif ()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (GIGESIM  DEFAULT_MSG  GIGESIM_INCLUDE_DIR GIGESIM_LIBRARIES)
