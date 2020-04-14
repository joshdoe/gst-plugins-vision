# - Try to find National Instruments IMAQdx
# Once done this will define
#
#  NIIMAQDX_FOUND - system has NI-IMAQdx
#  NIIMAQDX_INCLUDE_DIR - the NI-IMAQdx include directory
#  NIIMAQDX_LIBRARIES - the libraries needed to use NI-IMAQdx

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2010, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT NIIMAQDX_DIR)
    file(TO_CMAKE_PATH "$ENV{ProgramFiles}" _PROG_FILES)
    set(_PROG_FILES_X86 "ProgramFiles(x86)")
    file(TO_CMAKE_PATH "$ENV{${_PROG_FILES_X86}}" _PROG_FILES_X86)
    if (_PROG_FILES_X86)
        set(_PROGFILESDIR "${_PROG_FILES_X86}")
    else ()
        set(_PROGFILESDIR "${_PROG_FILES}")
    endif ()

    set (NIIMAQDX_DIR "${_PROGFILESDIR}/National Instruments" CACHE PATH "Directory containing NI includes and libraries")
endif ()

find_path (NIIMAQDX_INCLUDE_DIR NIIMAQdx.h
    PATHS
    "${NIIMAQDX_DIR}/Shared/ExternalCompilerSupport/C/Include"
    DOC "Directory containing NIIMAQdx.h include file")

find_library (NIIMAQDX_LIBRARIES NAMES niimaqdx
    PATHS
    "${NIIMAQDX_DIR}/Shared/ExternalCompilerSupport/C/Lib32/MSVC"
    DOC "NI-IMAQdx library to link with")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (NIIMAQDX  DEFAULT_MSG  NIIMAQDX_INCLUDE_DIR NIIMAQDX_LIBRARIES)

MARK_AS_ADVANCED(NIIMAQDX_INCLUDE_DIR NIIMAQDX_LIBRARIES)
