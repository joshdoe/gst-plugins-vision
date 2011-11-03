# - Try to find Active Silicon Phoenix SDK
# Once done this will define
#
#  PHOENIX_FOUND - system has Active Silicon Phoenix SDK
#  PHOENIX_INCLUDE_DIR - the Active Silicon Phoenix SDK include directory
#  PHOENIX_LIBRARIES - the libraries needed to use Active Silicon Phoenix SDK
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

# TODO: properly handle 32-/64-bit differences, and Linux

IF (PHOENIX_INCLUDE_DIR AND PHOENIX_LIBRARIES)
   # in cache already
   SET(PHOENIX_FIND_QUIETLY TRUE)
ELSE (PHOENIX_INCLUDE_DIR AND PHOENIX_LIBRARIES)
   SET(PHOENIX_FIND_QUIETLY FALSE)
ENDIF (PHOENIX_INCLUDE_DIR AND PHOENIX_LIBRARIES)

IF (NOT PHOENIX_DIR)
    SET (PHOENIX_DIR "C:/Program Files (x86)/Active Silicon/Phoenix/Win/SDK5.83" CACHE PATH "Directory containing the Active Silicon Phoenix SDK")
ENDIF (NOT PHOENIX_DIR)

FIND_PATH (PHOENIX_INCLUDE_DIR phx_api.h
    PATHS
    "${PHOENIX_DIR}/Include"
	"C:/Program Files (x86)/Active Silicon/Phoenix/Win/SDK5.83/Include"
	"C:/Program Files/Active Silicon/Phoenix/Win/SDK5.83/Include"
    DOC "Directory containing phx_api.h include file")

FIND_LIBRARY (PHOENIX_LIBRARIES NAMES phxlw32
    PATHS
    "${PHOENIX_DIR}/Lib/win32"
	"C:/Program Files (x86)/Active Silicon/Phoenix/Win/SDK5.83/Lib/win32"
	"C:/Program Files/Active Silicon/Phoenix/Win/SDK5.83/Lib/win32"
    DOC "PHOENIX library to link with")

IF (PHOENIX_INCLUDE_DIR)
   #MESSAGE(STATUS "DEBUG: Found Active Silicon Phoenix include dir: ${PHOENIX_INCLUDE_DIR}")
ELSE (PHOENIX_INCLUDE_DIR)
   MESSAGE(STATUS "PHOENIX: WARNING: include dir not found")
ENDIF (PHOENIX_INCLUDE_DIR)

IF (PHOENIX_LIBRARIES)
   #MESSAGE(STATUS "DEBUG: Found Active Silicon Phoenix library: ${PHOENIX_LIBRARIES}")
ELSE (PHOENIX_LIBRARIES)
   MESSAGE(STATUS "PHOENIX: WARNING: library not found")
ENDIF (PHOENIX_LIBRARIES)

INCLUDE (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (PHOENIX  DEFAULT_MSG  PHOENIX_INCLUDE_DIR PHOENIX_LIBRARIES)

MARK_AS_ADVANCED(PHOENIX_INCLUDE_DIR PHOENIX_LIBRARIES)