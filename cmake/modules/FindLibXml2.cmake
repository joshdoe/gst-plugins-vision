if (NOT LIBXML2_DIR)
    set (LIBXML2_DIR "" CACHE PATH "Directory containing libxml")
endif ()

FILE(TO_CMAKE_PATH "$ENV{LIBXML2_DIR}" TRY1_DIR)
FILE(TO_CMAKE_PATH "${LIBXML2_DIR}" TRY2_DIR)
FILE(GLOB LIBXML2_DIR ${TRY1_DIR} ${TRY2_DIR})

FIND_PATH(LIBXML2_INCLUDE_DIR libxml/parser.h
                              PATHS ${LIBXML2_DIR}/include ${LIBXML2_DIR}/include/libxml2 /usr/local/include/libxml2 /usr/include/libxml2
                              ENV INCLUDE DOC "Directory containing libxml/parser.h include file")
mark_as_advanced (LIBXML2_INCLUDE_DIR)

IF (LIBXML2_INCLUDE_DIR)
  SET(LIBXML2_FOUND TRUE)
ENDIF (LIBXML2_INCLUDE_DIR)
