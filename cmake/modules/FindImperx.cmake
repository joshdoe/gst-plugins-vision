# - Try to find Imperx FrameLink Express
# Once done this will define
#
#  IMPERX_FLEX_FOUND - system has Imperx FrameLink Express
#  IMPERX_FLEX_INCLUDE_DIR - the Imperx FrameLink Express include directory
#  IMPERX_FLEX_LIBRARIES - the libraries needed to use Imperx FrameLink Express

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2015, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT IMPERX_FLEX_DIR)
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
	
	set (IMPERX_FLEX_DIR "${_PROGFILESDIR}/Imperx/FrameLink Express" CACHE PATH "Directory containing Imperx FrameLink Express includes and libraries")
	
	if (CMAKE_SIZEOF_VOID_P MATCHES "8")
        set(_LIB_PATH "${IMPERX_FLEX_DIR}/SDK/lib/x64")
    else ()
        set(_LIB_PATH "${IMPERX_FLEX_DIR}/SDK/lib/win32")
    endif ()
endif ()

find_path (IMPERX_FLEX_INCLUDE_DIR VCECLB.h
    PATHS
    "${IMPERX_FLEX_DIR}/SDK/inc"
    DOC "Directory containing VCECLB.h include file")

find_library (IMPERX_FLEX_LIBRARIES NAMES VCECLB
    PATHS
    "${_LIB_PATH}"
    DOC "Imperx FrameLink Express library to link with")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (IMPERX_FLEX  DEFAULT_MSG  IMPERX_FLEX_INCLUDE_DIR IMPERX_FLEX_LIBRARIES)