# - Try to find Imperx FrameLink Express
# Once done this will define
#
#  IMPERX_SDI_FOUND - system has Imperx SDI
#  IMPERX_SDI_INCLUDE_DIR - the Imperx SDI include directory
#  IMPERX_SDI_LIBRARIES - the libraries needed to use Imperx SDI

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2019, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT IMPERX_SDI_DIR)
    # 32-bit dir on win32
    file(TO_CMAKE_PATH "$ENV{ProgramFiles}" _PROG_FILES)
    # 32-bit dir on win64
    set(_PROG_FILES_X86 "ProgramFiles(x86)")
    file(TO_CMAKE_PATH "$ENV{${_PROG_FILES_X86}}" _PROG_FILES_X86)
    # 64-bit dir on win64
    file(TO_CMAKE_PATH "$ENV{ProgramW6432}" _PROG_FILES_W6432)
    if (_PROG_FILES_X86)
        set(_PROGFILESDIR "${_PROG_FILES_W6432}")
    else ()
        set(_PROGFILESDIR "${_PROG_FILES}")
    endif ()
	
    set (IMPERX_SDI_DIR "${_PROGFILESDIR}/Imperx/HD-SDI Express" CACHE PATH "Directory containing Imperx HD-SDI Express includes and libraries")
	
	if (CMAKE_SIZEOF_VOID_P MATCHES "8")
        set(_LIB_PATH "${IMPERX_SDI_DIR}/SDK/lib/x64")
    else ()
        set(_LIB_PATH "${IMPERX_SDI_DIR}/SDK/lib/win32")
    endif ()
endif ()

find_path (IMPERX_SDI_INCLUDE_DIR VCESDI.h
    PATHS
    "${IMPERX_SDI_DIR}/SDK/inc"
    DOC "Directory containing VCESDI.h include file")

find_library (IMPERX_SDI_LIBRARIES NAMES VCESDI
    PATHS
    "${_LIB_PATH}"
    DOC "Imperx HD-SDI Express library to link with")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (IMPERX_SDI  DEFAULT_MSG  IMPERX_SDI_INCLUDE_DIR IMPERX_SDI_LIBRARIES)
