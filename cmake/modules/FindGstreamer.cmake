FILE(TO_CMAKE_PATH "$ENV{GSTREAMER_DIR}" TRY1_DIR)
FILE(TO_CMAKE_PATH "${GSTREAMER_DIR}" TRY2_DIR)
FILE(GLOB GSTREAMER_DIR ${TRY1_DIR} ${TRY2_DIR})
message(STATUS ": GSTREAMER_DIR=${GSTREAMER_DIR}")

FIND_PATH(GSTREAMER_gst_INCLUDE_DIR gst/gst.h
                                    PATHS ${GSTREAMER_DIR}/include ${GSTREAMER_DIR}/include/gstreamer-0.10 /usr/local/include/gstreamer-0.10 /usr/include/gstreamer-0.10  C:/gstreamer/include/gstreamer-0.10/gst
                                    ENV INCLUDE DOC "Directory containing gst/gst.h include file")
message(STATUS "GSTREAMER_gst_INCLUDE_DIR = ${GSTREAMER_gst_INCLUDE_DIR}")

FIND_PATH(GSTREAMER_gstconfig_INCLUDE_DIR gst/gstconfig.h
                                          PATHS ${GSTREAMER_DIR}/include ${GSTREAMER_DIR}/lib/include ${GSTREAMER_DIR}/include/gstreamer-0.10 /usr/local/include/gstreamer-0.10 /usr/include/gstreamer-0.10 /usr/local/lib/include/gstreamer-0.10 /usr/lib/include/gstreamer-0.10
                                          ENV INCLUDE DOC "Directory containing gst/gstconfig.h include file")
message(STATUS "GSTREAMER_gstconfig_INCLUDE_DIR = ${GSTREAMER_gstconfig_INCLUDE_DIR}")

FIND_LIBRARY(GSTREAMER_gstaudio_LIBRARY NAMES gstaudio-0.10 libgstaudio-0.10 gstaudio
                                        PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                        ENV LIB
                                        DOC "gstaudio library to link with"
                                        NO_SYSTEM_ENVIRONMENT_PATH)
message(STATUS "GSTREAMER_gstaudio_LIBRARY = ${GSTREAMER_gstaudio_LIBRARY}")

FIND_LIBRARY(GSTREAMER_gstbase_LIBRARY NAMES gstbase-0.10 libgstbase-0.10 gstbase
                                       PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                       ENV LIB
                                       DOC "gstbase library to link with"
                                       NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GLIB_gstcdda_LIBRARY NAMES gstcdda-0.10 libgstcdda-0.10 gstcdda
                                  PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                  ENV LIB
                                  DOC "gstcdda library to link with"
                                  NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GSTREAMER_gstcontroller_LIBRARY NAMES gstcontroller-0.10 libgstcontroller-0.10 gstcontroller
                                             PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                             ENV LIB
                                             DOC "gstcontroller library to link with"
                                             NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GSTREAMER_gstdataprotocol_LIBRARY NAMES gstdataprotocol-0.10 libgstdataprotocol-0.10 gstdataprotocol
                                               PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                               ENV LIB
                                               DOC "gstdataprotocol library to link with"
                                               NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GSTREAMER_gstinterfaces_LIBRARY NAMES gstinterfaces-0.10 libgstinterfaces-0.10 gstinterfaces
                                             PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                             ENV LIB
                                             DOC "gstinterfaces library to link with"
                                             NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GSTREAMER_gstnet_LIBRARY NAMES gstnet-0.10 libgstnet-0.10 gstnet
                                      PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                      ENV LIB
                                      DOC "gstnet library to link with"
                                      NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GSTREAMER_gstnetbuffer_LIBRARY NAMES gstnetbuffer-0.10 libgstnetbuffer-0.10 gstnetbuffer
                                            PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                            ENV LIB
                                            DOC "gstnetbuffer library to link with"
                                            NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GSTREAMER_gstpbutils_LIBRARY NAMES gstpbutils-0.10 libgstpbutils-0.10 gstpbutils
                                          PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                          ENV LIB
                                          DOC "gstpbutils library to link with"
                                          NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GSTREAMER_gstreamer_LIBRARY NAMES gstreamer-0.10 libgstreamer-0.10 gstreamer
                                         PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                         ENV LIB
                                         DOC "gstreamer library to link with"
                                         NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GSTREAMER_gstriff_LIBRARY NAMES gstriff-0.10 libgstriff-0.10 gstriff
                                             PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                             ENV LIB
                                             DOC "gstriff library to link with"
                                             NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GSTREAMER_gstrtp_LIBRARY NAMES gstrtp-0.10 libgstrtp-0.10 gstrtp
                                    PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                    ENV LIB
                                    DOC "gstrtp library to link with"
                                    NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GSTREAMER_gstrtsp_LIBRARY NAMES gstrtsp-0.10 libgstrtsp-0.10 gstrtsp
                                       PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                       ENV LIB
                                       DOC "gstrtsp library to link with"
                                       NO_SYSTEM_ENVIRONMENT_PATH)

#FIND_LIBRARY(GSTREAMER_gstsdp_LIBRARY NAMES gstsdp-0.10 libgstsdp-0.10
#                                      PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
#                                      ENV LIB
#                                      DOC "gstsdp library to link with"
#                                      NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GSTREAMER_gsttag_LIBRARY NAMES gsttag-0.10 libgsttag-0.10 gsttag
                                 PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                 ENV LIB
                                 DOC "gsttag library to link with"
                                 NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GSTREAMER_gstvideo_LIBRARY NAMES gstvideo-0.10 libgstvideo-0.10 gstvideo
                                        PATHS ${GSTREAMER_DIR}/bin ${GSTREAMER_DIR}/win32/bin ${GSTREAMER_DIR}/bin/bin C:/gstreamer/bin ${GSTREAMER_DIR}/lib ${GSTREAMER_DIR}/win32/lib /usr/local/lib /usr/lib
                                        ENV LIB
                                        DOC "gstvideo library to link with"
                                        NO_SYSTEM_ENVIRONMENT_PATH)


#IF (GSTREAMER_gst_INCLUDE_DIR AND GSTREAMER_gstconfig_INCLUDE_DIR AND
#    GSTREAMER_gstaudio_LIBRARY AND GSTREAMER_gstbase_LIBRARY AND GSTREAMER_gstcontroller_LIBRARY AND
#    GSTREAMER_gstdataprotocol_LIBRARY AND GSTREAMER_gstinterfaces_LIBRARY AND GSTREAMER_gstnet_LIBRARY AND
#    GSTREAMER_gstnetbuffer_LIBRARY AND GSTREAMER_gstpbutils_LIBRARY AND GSTREAMER_gstreamer_LIBRARY AND
#    GSTREAMER_gstriff_LIBRARY AND GSTREAMER_gstrtp_LIBRARY AND GSTREAMER_gstrtsp_LIBRARY AND GSTREAMER_gstsdp_LIBRARY AND
#    GSTREAMER_gsttag_LIBRARY AND GSTREAMER_gstvideo_LIBRARY)
  SET(GSTREAMER_INCLUDE_DIR ${GSTREAMER_gst_INCLUDE_DIR} ${GSTREAMER_gstconfig_INCLUDE_DIR})
  list(REMOVE_DUPLICATES GSTREAMER_INCLUDE_DIR)
  SET(GSTREAMER_LIBRARIES ${GSTREAMER_gstaudio_LIBRARY} ${GSTREAMER_gstbase_LIBRARY}
                          ${GSTREAMER_gstcontroller_LIBRARY} ${GSTREAMER_gstdataprotocol_LIBRARY} ${GSTREAMER_gstinterfaces_LIBRARY}
                          ${GSTREAMER_gstnet_LIBRARY} ${GSTREAMER_gstnetbuffer_LIBRARY} ${GSTREAMER_gstpbutils_LIBRARY}
                          ${GSTREAMER_gstreamer_LIBRARY} ${GSTREAMER_gstriff_LIBRARY} ${GSTREAMER_gstrtp_LIBRARY}
                          ${GSTREAMER_gstrtsp_LIBRARY} ${GSTREAMER_gstsdp_LIBRARY} ${GSTREAMER_gsttag_LIBRARY} ${GSTREAMER_gstvideo_LIBRARY})
  list(REMOVE_DUPLICATES GSTREAMER_LIBRARIES)
  SET(GSTREAMER_FOUND TRUE)
#ENDIF (GSTREAMER_gst_INCLUDE_DIR AND GSTREAMER_gstconfig_INCLUDE_DIR AND
#       GSTREAMER_gstaudio_LIBRARY AND GSTREAMER_gstbase_LIBRARY AND GSTREAMER_gstcontroller_LIBRARY AND
#       GSTREAMER_gstdataprotocol_LIBRARY AND GSTREAMER_gstinterfaces_LIBRARY AND GSTREAMER_gstnet_LIBRARY AND
#       GSTREAMER_gstnetbuffer_LIBRARY AND GSTREAMER_gstpbutils_LIBRARY AND GSTREAMER_gstreamer_LIBRARY AND
#       GSTREAMER_gstriff_LIBRARY AND GSTREAMER_gstrtp_LIBRARY AND GSTREAMER_gstrtsp_LIBRARY AND GSTREAMER_gstsdp_LIBRARY AND
#       GSTREAMER_gsttag_LIBRARY AND GSTREAMER_gstvideo_LIBRARY)
