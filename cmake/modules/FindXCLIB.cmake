# - Try to find EPIX XCLIB
# Once done this will define
#
#  XCLIB_FOUND - system has EPIX XCLIB
#  XCLIB_INCLUDE_DIR - the EPIX XCLIB include directory
#  XCLIB_LIBRARIES - the libraries needed to use EPIX XCLIB
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

# TODO: properly handle Linux

if (NOT XCLIB_DIR)
    # 32-bit dir on win32
    file(TO_CMAKE_PATH "$ENV{ProgramFiles}" _PROG_FILES)
    # 32-bit dir on win64
    file(TO_CMAKE_PATH "$ENV{ProgramFiles(x86)}" _PROG_FILES_X86)
    # 64-bit dir on win64
    file(TO_CMAKE_PATH "$ENV{ProgramW6432}" _PROG_FILES_W6432)
    
    if (CMAKE_SIZEOF_VOID_P MATCHES "8")
        set(_PROGFILESDIR "${_PROG_FILES_W6432}")
    else ()
        if (_PROG_FILES_X86)
            set(_PROGFILESDIR "${_PROG_FILES_X86}")
        else ()
            set(_PROGFILESDIR "${_PROG_FILES}")
        endif ()
    endif ()

    set (XCLIB_DIR "${_PROGFILESDIR}/EPIX/XCLIB" CACHE PATH "Directory containing EPIX PIXCI XCLIB includes and libraries")
endif (NOT XCLIB_DIR)

find_path (XCLIB_INCLUDE_DIR xcliball.h
    PATHS
    "${XCLIB_DIR}"
    DOC "Directory containing xcliball.h include file")

if (CMAKE_SIZEOF_VOID_P MATCHES "8")
    find_library (XCLIB_LIBRARIES NAMES XCLIBW64
        PATHS
        "${XCLIB_DIR}"
        DOC "XCLIB 64-bit library to link with")
else ()
    find_library (XCLIB_LIBRARIES NAMES XCLIBWNT
        PATHS
        "${XCLIB_DIR}"
        DOC "XCLIB 32-bit library to link with")
endif ()

if (XCLIB_INCLUDE_DIR)
   #message(STATUS "DEBUG: Found EPIX XCLIB include dir: ${XCLIB_INCLUDE_DIR}")
else (XCLIB_INCLUDE_DIR)
   message(STATUS "XCLIB: WARNING: include dir not found")
endif (XCLIB_INCLUDE_DIR)

if (XCLIB_LIBRARIES)
   #message(STATUS "DEBUG: Found EPIX XCLIB library: ${XCLIB_LIBRARIES}")
else (XCLIB_LIBRARIES)
   message(STATUS "XCLIB: WARNING: library not found")
endif (XCLIB_LIBRARIES)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (XCLIB  DEFAULT_MSG  XCLIB_INCLUDE_DIR XCLIB_LIBRARIES)

mark_as_advanced(XCLIB_INCLUDE_DIR XCLIB_LIBRARIES)
