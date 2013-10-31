# - Try to find EDT API
# Once done this will define
#
#  EDT_FOUND - system has EDT API
#  EDT_INCLUDE_DIR - the EDT API include directory
#  EDT_LIBRARIES - the libraries needed to use the EDT API

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

IF (EDT_INCLUDE_DIR AND EDT_LIBRARIES)
   # in cache already
   SET(EDT_FIND_QUIETLY TRUE)
ELSE (EDT_INCLUDE_DIR AND EDT_LIBRARIES)
   SET(EDT_FIND_QUIETLY FALSE)
ENDIF (EDT_INCLUDE_DIR AND EDT_LIBRARIES)

IF (NOT EDT_DIR)
    SET (EDT_DIR "C:/EDT" CACHE PATH "Directory containing EDT API")
ENDIF (NOT EDT_DIR)

FIND_PATH (EDT_INCLUDE_DIR edtinc.h
    PATHS
    "${EDT_DIR}/pdv"
    DOC "Directory containing edtinc.h include file")

FIND_LIBRARY (EDT_LIBRARIES NAMES pdvlib
    PATHS
    "${EDT_DIR}/lib"
    DOC "EDT library to link with")

IF (EDT_INCLUDE_DIR)
   #MESSAGE(STATUS "DEBUG: Found Euresys Multicam include dir: ${EDT_INCLUDE_DIR}")
ELSE (EDT_INCLUDE_DIR)
   MESSAGE(STATUS "EDT: WARNING: include dir not found")
ENDIF (EDT_INCLUDE_DIR)

IF (EDT_LIBRARIES)
   #MESSAGE(STATUS "DEBUG: Found Euresys Multicam library: ${EDT_LIBRARIES}")
ELSE (EDT_LIBRARIES)
   MESSAGE(STATUS "EDT: WARNING: library not found")
ENDIF (EDT_LIBRARIES)

INCLUDE (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (EDT  DEFAULT_MSG  EDT_INCLUDE_DIR EDT_LIBRARIES)

MARK_AS_ADVANCED(EDT_INCLUDE_DIR EDT_LIBRARIES)