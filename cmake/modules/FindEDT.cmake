# - Try to find EDT PDV
# Once done this will define
#
#  EDT_FOUND - system has EDT PDV
#  EDT_INCLUDE_DIR - the EDT PDV include directory
#  EDT_LIBRARIES - the libraries needed to use the EDT PDV

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOT EDT_DIR)
    set (EDT_DIR "C:/EDT" CACHE PATH "Directory containing EDT PDV")
endif (NOT EDT_DIR)

find_path (EDT_INCLUDE_DIR edtinc.h
    PATHS
    "${EDT_DIR}/pdv"
    DOC "Directory containing edtinc.h include file")

if (CMAKE_SIZEOF_VOID_P MATCHES "8")
find_library (EDT_LIBRARIES NAMES pdvlib
    PATHS
    "${EDT_DIR}/pdv/lib/amd64"
    DOC "EDT library to link with")
else ()
find_library (EDT_LIBRARIES NAMES pdvlib
    PATHS
    "${EDT_DIR}/pdv/lib/x86"
    DOC "EDT library to link with")
endif ()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (EDT  DEFAULT_MSG  EDT_INCLUDE_DIR EDT_LIBRARIES)