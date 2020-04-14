# - Try to find Active Silicon Phoenix SDK
# Once done this will define
#
#  PHOENIX_FOUND - system has Active Silicon Phoenix SDK
#  PHOENIX_INCLUDE_DIR - the Active Silicon Phoenix SDK include directory
#  PHOENIX_LIBRARIES - the libraries needed to use Active Silicon Phoenix SDK
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT PHOENIX_DIR)
    set (PHOENIX_DIR "C:/Program Files (x86)/Active Silicon/Phoenix/Win/SDK6.30" CACHE PATH "Directory containing Phoenix SDK includes and libraries")
endif ()

find_path(PHOENIX_INCLUDE_DIR phx_api.h
    PATHS
    "${PHOENIX_DIR}/Include"
    DOC "Directory containing phx_api.h include file")

if (CMAKE_SIZEOF_VOID_P MATCHES "8")
	find_library(PHOENIX_LIBRARIES NAMES phxlx64
		PATHS
		"${PHOENIX_DIR}/Lib/win64"
		DOC "PHOENIX library to link with")
else ()
	find_library(PHOENIX_LIBRARIES NAMES phxlw32
		PATHS
		"${PHOENIX_DIR}/Lib/win32"
		DOC "PHOENIX library to link with")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PHOENIX  DEFAULT_MSG  PHOENIX_INCLUDE_DIR PHOENIX_LIBRARIES)
