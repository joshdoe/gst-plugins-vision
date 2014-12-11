# - Try to find Sapera SDK
# Once done this will define
#
#  SAPERA_FOUND - system has Sapera SDK
#  SAPERA_INCLUDE_DIR - the Sapera SDK include directory
#  SAPERA_LIBRARIES - the libraries needed to use the Sapera SDK

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


if (NOT SAPERA_DIR)
    # 32-bit dir on win32
    file(TO_CMAKE_PATH "$ENV{ProgramFiles}" _PROG_FILES)
    # 32-bit dir on win64
    file(TO_CMAKE_PATH "$ENV{ProgramFiles(x86)}" _PROG_FILES_X86)
    # 64-bit dir on win64
    file(TO_CMAKE_PATH "$ENV{ProgramW6432}" _PROG_FILES_W6432)
    if (_PROG_FILES_X86)
        set(_PROGFILESDIR "${_PROG_FILES_W6432}")
    else ()
        set(_PROGFILESDIR "${_PROG_FILES}")
    endif ()

    set (SAPERA_DIR "${_PROGFILESDIR}/Teledyne DALSA/Sapera" CACHE PATH "Directory containing NI includes and libraries")
endif ()

find_path(SAPERA_PLUS_PLUS_INCLUDE_DIR SapClassBasic.h
    PATHS
    "${SAPERA_DIR}/Classes/Basic"
    DOC "Directory containing Sapera include files")
	
find_path(SAPERA_C_INCLUDE_DIR corapi.h
    PATHS
    "${SAPERA_DIR}/Include"
    DOC "Directory containing Sapera include files")

set (SAPERA_INCLUDE_DIR ${SAPERA_PLUS_PLUS_INCLUDE_DIR} ${SAPERA_C_INCLUDE_DIR})
	
if (CMAKE_SIZEOF_VOID_P MATCHES "8")
	find_library(SAPERA_LIBRARIES NAMES SapClassBasic
		PATHS
		"${SAPERA_DIR}/Lib/Win64"
		DOC "Sapera library to link with")
else ()
	find_library(SAPERA_LIBRARIES NAMES SapClassBasic
		PATHS
		"${SAPERA_DIR}/Lib/Win32"
		DOC "Sapera library to link with")
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SAPERA DEFAULT_MSG SAPERA_INCLUDE_DIR SAPERA_LIBRARIES)