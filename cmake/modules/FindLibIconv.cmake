FILE(TO_CMAKE_PATH "$ENV{LIBICONV_DIR}" TRY1_DIR)
FILE(TO_CMAKE_PATH "${LIBICONV_DIR}" TRY2_DIR)
FILE(GLOB LIBICONV_DIR ${TRY1_DIR} ${TRY2_DIR})

FIND_PATH(LIBICONV_INCLUDE_DIR iconv.h
                               PATHS ${LIBICONV_DIR}/include /usr/local/include /usr/include
                               ENV INCLUDE DOC "Directory containing iconv.h include file")

IF (LIBICONV_INCLUDE_DIR)
  SET(LIBICONV_FOUND TRUE)
ENDIF (LIBICONV_INCLUDE_DIR)
