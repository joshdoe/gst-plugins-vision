# - Try to find Xiris SDK
# Once done this will define
#
#  XIRIS_FOUND - system has Xiris SDK
#  XIRIS_INCLUDE_DIR - the Xiris SDK include directory
#  XIRIS_LIBRARIES - the libraries needed to use Xiris SDK

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

set (VERSION "2.0.6")

if (NOT XIRIS_DIR)
  # if (WIN32)
    # TODO: support Xiris WeldSDK on Windows
  # else ()
    set (_XIRIS_DIR "/opt/xiris/weldsdk-${VERSION}")
  # endif ()
    set (XIRIS_DIR ${_XIRIS_DIR} CACHE PATH "Directory containing Xiris SDK includes and libraries")
endif ()

find_path (XIRIS_INCLUDE_DIR WeldSDK/WeldSDK.h
    PATHS
    "${XIRIS_DIR}/include"
    DOC "Directory containing WeldSDK.h include file")

# set(LIB_NAMES WeldSDK
#               avcodec
#               avdevice
#               avfilter
#               avformat
#               avutil
#               EbTransportLayerLib
#               EbUtilsLib
#               kfunc64
#               PtConvertersLib
#               PtUtilsLib
#               PvAppUtils
#               PvBase
#               PvBuffer
#               PvCameraBridge
#               PvDevice
#               PvGenICam
#               PvGUI
#               PvPersistence
#               PvSerial
#               PvStream
#               PvSystem
#               PvTransmitter
#               PvVirtualDevice
#               rtaudio
#               SimpleImagingLib
#               swresample
#               swscale
#               XVideoStream
#               # opencv_core
#               # opencv_highgui
#               )


find_library (_WeldSDK NAMES WeldSDK
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_avcodec NAMES avcodec
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_avdevice NAMES avdevice
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_avfilter NAMES avfilter
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_avformat NAMES avformat
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_avutil NAMES avutil
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_EbTransportLayerLib NAMES EbTransportLayerLib
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_EbUtilsLib NAMES EbUtilsLib
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_kfunc64 NAMES kfunc64
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PtConvertersLib NAMES PtConvertersLib
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PtUtilsLib NAMES PtUtilsLib
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvAppUtils NAMES PvAppUtils
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvBase NAMES PvBase
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvBuffer NAMES PvBuffer
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvCameraBridge NAMES PvCameraBridge
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvDevice NAMES PvDevice
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvGenICam NAMES PvGenICam
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvGUI NAMES PvGUI
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvPersistence NAMES PvPersistence
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvSerial NAMES PvSerial
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvStream NAMES PvStream
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvSystem NAMES PvSystem
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvTransmitter NAMES PvTransmitter
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_PvVirtualDevice NAMES PvVirtualDevice
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_rtaudio NAMES rtaudio
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_SimpleImagingLib NAMES SimpleImagingLib
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_swresample NAMES swresample
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_swscale NAMES swscale
    PATHS
    "${XIRIS_DIR}/lib"
    )
find_library (_XVideoStream NAMES XVideoStream
    PATHS
    "${XIRIS_DIR}/lib"
    )
# find_library (_stdc++fs NAMES stdc++fs
#     PATHS
#     "${XIRIS_DIR}/lib"
#     )

set (XIRIS_LIBRARIES ${_WeldSDK}
                    ${_avcodec}
                    ${_avdevice}
                    ${_avfilter}
                    ${_avformat}
                    ${_avutil}
                    ${_EbTransportLayerLib}
                    ${_EbUtilsLib}
                    ${_kfunc64}
                    ${_PtConvertersLib}
                    ${_PtUtilsLib}
                    ${_PvAppUtils}
                    ${_PvBase}
                    ${_PvBuffer}
                    ${_PvCameraBridge}
                    ${_PvDevice}
                    ${_PvGenICam}
                    ${_PvGUI}
                    ${_PvPersistence}
                    ${_PvSerial}
                    ${_PvStream}
                    ${_PvSystem}
                    ${_PvTransmitter}
                    ${_PvVirtualDevice}
                    ${_rtaudio}
                    ${_SimpleImagingLib}
                    ${_swresample}
                    ${_swscale}
                    ${_XVideoStream})

# set (XIRIS_LIBRARIES)
# foreach (LIB ${LIB_NAMES})
#   set (LIB_VAR "${LIB}")
#   find_library (${LIB_VAR} ${LIB} PATHS "${XIRIS_DIR}/lib")
#   list (APPEND XIRIS_LIBRARIES ${LIB_VAR})
# endforeach (LIB)

# set (XIRIS_LIBRARIES ${_XirisCppLib})

mark_as_advanced (XIRIS_LIBRARIES)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (XIRIS  DEFAULT_MSG  XIRIS_INCLUDE_DIR XIRIS_LIBRARIES)

