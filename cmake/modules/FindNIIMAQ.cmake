# - Try to find National Instruments IMAQ
# Once done this will define
#
#  NIIMAQ_FOUND - system has NI-IMAQ
#  NIIMAQ_INCLUDE_DIR - the NI-IMAQ include directory
#  NIIMAQ_LIBRARIES - the libraries needed to use NI-IMAQ

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2010, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

IF (NIIMAQ_INCLUDE_DIR AND NIIMAQ_LIBRARIES)
   # in cache already
   SET(NIIMAQ_FIND_QUIETLY TRUE)
ELSE (NIIMAQ_INCLUDE_DIR AND NIIMAQ_LIBRARIES)
   SET(NIIMAQ_FIND_QUIETLY FALSE)
ENDIF (NIIMAQ_INCLUDE_DIR AND NIIMAQ_LIBRARIES)

IF (NOT NIIMAQ_DIR)
    SET (NIIMAQ_DIR "C:/Program Files/National Instruments" CACHE PATH "Directory containing NI includes and libraries")
ENDIF (NOT NIIMAQ_DIR)

FIND_PATH (NIIMAQ_INCLUDE_DIR niimaq.h
    PATHS
    "${NIIMAQ_DIR}/Shared/ExternalCompilerSupport/C/Include"
    DOC "Directory containing niimaq.h include file")

FIND_LIBRARY (NIIMAQ_LIBRARIES NAMES imaq
    PATHS
    "${NIIMAQ_DIR}/Shared/ExternalCompilerSupport/C/Lib32/MSVC"
    DOC "niimaq library to link with")

IF (NIIMAQ_INCLUDE_DIR)
   #MESSAGE(STATUS "DEBUG: Found NI-IMAQ include dir: ${NIIMAQ_INCLUDE_DIR}")
ELSE (NIIMAQ_INCLUDE_DIR)
   MESSAGE(STATUS "NI-IMAQ: WARNING: include dir not found")
ENDIF (NIIMAQ_INCLUDE_DIR)

IF (NIIMAQ_LIBRARIES)
   #MESSAGE(STATUS "DEBUG: Found NI-IMAQ library: ${NIIMAQ_LIBRARIES}")
ELSE (NIIMAQ_LIBRARIES)
   MESSAGE(STATUS "NI-IMAQ: WARNING: library not found")
ENDIF (NIIMAQ_LIBRARIES)

INCLUDE (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (NIIMAQ  DEFAULT_MSG  NIIMAQ_INCLUDE_DIR NIIMAQ_LIBRARIES)

MARK_AS_ADVANCED(NIIMAQ_INCLUDE_DIR NIIMAQ_LIBRARIES)