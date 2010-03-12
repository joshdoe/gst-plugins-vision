FILE(TO_CMAKE_PATH "$ENV{PNG_DIR}" TRY1_DIR)
FILE(TO_CMAKE_PATH "${PNG_DIR}" TRY2_DIR)
FILE(GLOB PNG_DIR ${TRY1_DIR} ${TRY2_DIR})

FIND_PATH(PNG_INCLUDE_DIR png.h
                           PATHS ${PNG_DIR}/include /usr/local/include /usr/include
                           ENV INCLUDE DOC "Directory containing png.h include file")

FIND_LIBRARY(PNG_LIBRARY NAMES png12 png
                          PATHS ${PNG_DIR}/bin ${PNG_DIR}/win32/bin ${PNG_DIR}/lib ${PNG_DIR}/win32/lib /usr/local/lib /usr/lib
                          ENV LIB
                          DOC "png library to link with"
                          NO_SYSTEM_ENVIRONMENT_PATH)

IF (PNG_INCLUDE_DIR AND PNG_LIBRARY)
   SET(PNG_FOUND TRUE)
ENDIF (PNG_INCLUDE_DIR AND PNG_LIBRARY)
