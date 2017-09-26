# - Try to find Euresys Multicam
# Once done this will define
#
#  EURESYS_FOUND - system has Euresys Multicam
#  EURESYS_INCLUDE_DIR - the Euresys Multicam include directory
#  EURESYS_LIBRARIES - the libraries needed to use Euresys Multicam

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2015, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT EURESYS_DIR)
    # Euresys seems to be installed in the 32-bit dir on 32- or 64-bit Windows
    # 32-bit dir on win32
    file(TO_CMAKE_PATH "$ENV{ProgramFiles}" _PROG_FILES)
    # 32-bit dir on win64
    file(TO_CMAKE_PATH "$ENV{ProgramFiles(x86)}" _PROG_FILES_X86)

    # use (x86) dir if exists
    if (_PROG_FILES_X86)
        set(_PROG_FILES "${_PROG_FILES_X86}")
    endif ()

    set (EURESYS_DIR "${_PROG_FILES}/Euresys/MultiCam" CACHE PATH "Directory containing Euresys Multicam includes and libraries")

    if (CMAKE_SIZEOF_VOID_P MATCHES "8")
        set(_LIB_PATH "${EURESYS_DIR}/lib/amd64")
    else ()
        set(_LIB_PATH "${EURESYS_DIR}/lib")
    endif ()
endif ()

find_path (EURESYS_INCLUDE_DIR multicam.h
    PATHS
    "${EURESYS_DIR}/include"
    DOC "Directory containing multicam.h include file")

find_library (EURESYS_LIBRARIES NAMES MultiCam
    PATHS
    "${_LIB_PATH}"
    DOC "EURESYS library to link with")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (EURESYS  DEFAULT_MSG  EURESYS_INCLUDE_DIR EURESYS_LIBRARIES)