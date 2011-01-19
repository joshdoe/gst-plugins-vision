# - Try to find Euresys Multicam
# Once done this will define
#
#  EURESYS_FOUND - system has Euresys Multicam
#  EURESYS_INCLUDE_DIR - the Euresys Multicam include directory
#  EURESYS_LIBRARIES - the libraries needed to use Euresys Multicam

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2011, United States Government, Joshua M. Doe <oss@nvl.army.mil>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

IF (EURESYS_INCLUDE_DIR AND EURESYS_LIBRARIES)
   # in cache already
   SET(EURESYS_FIND_QUIETLY TRUE)
ELSE (EURESYS_INCLUDE_DIR AND EURESYS_LIBRARIES)
   SET(EURESYS_FIND_QUIETLY FALSE)
ENDIF (EURESYS_INCLUDE_DIR AND EURESYS_LIBRARIES)

IF (NOT EURESYS_DIR)
    SET (EURESYS_DIR "/usr/local/euresys/multicam" CACHE PATH "Directory containing Euresys Multicam includes and libraries")
ENDIF (NOT EURESYS_DIR)

FIND_PATH (EURESYS_INCLUDE_DIR multicam.h
    PATHS
    "${EURESYS_DIR}/include"
    DOC "Directory containing multicam.h include file")

FIND_LIBRARY (EURESYS_LIBRARIES NAMES libMultiCam
    PATHS
    "${EURESYS_DIR}/drivers"
    DOC "EURESYS library to link with")

IF (EURESYS_INCLUDE_DIR)
   #MESSAGE(STATUS "DEBUG: Found Euresys Multicam include dir: ${EURESYS_INCLUDE_DIR}")
ELSE (EURESYS_INCLUDE_DIR)
   MESSAGE(STATUS "EURESYS: WARNING: include dir not found")
ENDIF (EURESYS_INCLUDE_DIR)

IF (EURESYS_LIBRARIES)
   #MESSAGE(STATUS "DEBUG: Found Euresys Multicam library: ${EURESYS_LIBRARIES}")
ELSE (EURESYS_LIBRARIES)
   MESSAGE(STATUS "EURESYS: WARNING: library not found")
ENDIF (EURESYS_LIBRARIES)

INCLUDE (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (EURESYS  DEFAULT_MSG  EURESYS_INCLUDE_DIR EURESYS_LIBRARIES)

MARK_AS_ADVANCED(EURESYS_INCLUDE_DIR EURESYS_LIBRARIES)