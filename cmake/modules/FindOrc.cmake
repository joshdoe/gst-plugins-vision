# - Try to find the Orc libraries
# Once done this will define
#
#  ORC_FOUND - system has Orc
#  ORC_INCLUDE_DIR - the Orc include directory
#  ORC_LIBRARIES - Orc library

# Copyright (c) 2008 Laurent Montel, <montel@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


if (NOT WIN32)
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(PKG_ORC REQUIRED orc-0.4)
endif(NOT WIN32)

if (CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(GSTREAMER_ROOT $ENV{GSTREAMER_1_0_ROOT_X86_64})
else ()
    set(GSTREAMER_ROOT $ENV{GSTREAMER_1_0_ROOT_X86})
endif ()

find_path(ORC_INCLUDE_DIR orc/orc.h
          PATH_SUFFIXES orc-0.4
          HINTS ${PKG_ORC_INCLUDE_DIRS} ${PKG_ORC_INCLUDEDIR} ${GSTREAMER_ROOT}/include)

find_library(ORC_LIBRARIES
             NAMES orc-0.4
             HINTS ${PKG_ORC_LIBRARY_DIRS} ${PKG_ORC_LIBDIR} ${GSTREAMER_ROOT}/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ORC  DEFAULT_MSG  ORC_LIBRARIES ORC_INCLUDE_DIR)