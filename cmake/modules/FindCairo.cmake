FILE(TO_CMAKE_PATH "$ENV{CAIRO_DIR}" TRY1_DIR)
FILE(TO_CMAKE_PATH "${CAIRO_DIR}" TRY2_DIR)
FILE(GLOB CAIRO_DIR ${TRY1_DIR} ${TRY2_DIR})

FIND_PATH(CAIRO_INCLUDE_DIR cairo.h
                            PATHS ${CAIRO_DIR}/include /usr/local/include/cairo /usr/include/cairo
                            ENV INCLUDE DOC "Directory containing cairo.h include file")

IF (CAIRO_INCLUDE_DIR)
  SET(CAIRO_FOUND TRUE)
ENDIF (CAIRO_INCLUDE_DIR)
