# - Try to find National Instruments IMAQ
# Once done this will define
#
#  NIIMAQ_FOUND - system has NI-IMAQ
#  NIIMAQ_INCLUDE_DIR - the NI-IMAQ include directory
#  NIIMAQ_LIBRARIES - the libraries needed to use NI-IMAQ

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2014, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT NIIMAQ_DIR)
    # 32-bit dir on win32
    file(TO_CMAKE_PATH "$ENV{ProgramFiles}" _PROG_FILES)
    # 32-bit dir on win64
    file(TO_CMAKE_PATH "$ENV{ProgramFiles(x86)}" _PROG_FILES_X86)
    # 64-bit dir on win64
    file(TO_CMAKE_PATH "$ENV{ProgramW6432}" _PROG_FILES_W6432)
    
	# NI puts 64-bit lib in 32-bit Program Files directory
	if (_PROG_FILES_X86)
		set(_PROGFILESDIR "${_PROG_FILES_X86}")
	else ()
		set(_PROGFILESDIR "${_PROG_FILES}")
	endif ()

    set(NIIMAQ_DIR "${_PROGFILESDIR}/National Instruments" CACHE PATH "Top level National Instruments directory")
endif (NOT NIIMAQ_DIR)

find_path(NIIMAQ_INCLUDE_DIR niimaq.h
    PATHS
    "${NIIMAQ_DIR}/Shared/ExternalCompilerSupport/C/Include"
    DOC "Directory containing niimaq.h include file")

if (CMAKE_SIZEOF_VOID_P MATCHES "8")
	find_library(NIIMAQ_LIBRARIES NAMES imaq
		PATHS
		"${NIIMAQ_DIR}/Shared/ExternalCompilerSupport/C/Lib64/MSVC"
		DOC "niimaq library to link with")
else ()
	find_library(NIIMAQ_LIBRARIES NAMES imaq
		PATHS
		"${NIIMAQ_DIR}/Shared/ExternalCompilerSupport/C/Lib32/MSVC"
		DOC "niimaq library to link with")
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NIIMAQ DEFAULT_MSG NIIMAQ_INCLUDE_DIR NIIMAQ_LIBRARIES)

mark_as_advanced(NIIMAQ_INCLUDE_DIR NIIMAQ_LIBRARIES)