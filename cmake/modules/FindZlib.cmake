FILE(TO_CMAKE_PATH "$ENV{ZLIB_DIR}" TRY1_DIR)
FILE(TO_CMAKE_PATH "${ZLIB_DIR}" TRY2_DIR)
FILE(GLOB ZLIB_DIR ${TRY1_DIR} ${TRY2_DIR})

FIND_PATH(ZLIB_INCLUDE_DIR zlib.h
                           PATHS ${ZLIB_DIR}/include /usr/local/include /usr/include
                           ENV INCLUDE DOC "Directory containing zlib.h include file")

FIND_LIBRARY(ZLIB_LIBRARY NAMES z
                          PATHS ${ZLIB_DIR}/bin ${ZLIB_DIR}/win32/bin ${ZLIB_DIR}/lib ${ZLIB_DIR}/win32/lib /usr/local/lib /usr/lib
                          ENV LIB
                          DOC "zlib library to link with"
                          NO_SYSTEM_ENVIRONMENT_PATH)

IF (ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
   SET(ZLIB_FOUND TRUE)
ENDIF (ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
