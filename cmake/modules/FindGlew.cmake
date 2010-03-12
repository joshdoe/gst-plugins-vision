FILE(TO_CMAKE_PATH "$ENV{GLEW_DIR}" TRY1_DIR)
FILE(TO_CMAKE_PATH "${GLEW_DIR}" TRY2_DIR)
FILE(GLOB GLEW_DIR ${TRY1_DIR} ${TRY2_DIR})

FIND_PATH(GLEW_INCLUDE_DIR GL/glew.h
                           PATHS ${GLEW_DIR}/include /usr/local/include /usr/include
                           ENV INCLUDE DOC "Directory containing GL/glew.h include file")

FIND_LIBRARY(GLEW_LIBRARY NAMES glew32 GLEW glew32s
                          PATHS ${GLEW_DIR}/bin ${GLEW_DIR}/win32/bin ${GLEW_DIR}/lib ${GLEW_DIR}/win32/lib /usr/local/lib /usr/lib
                          ENV LIB
                          DOC "glew library to link with"
                          NO_SYSTEM_ENVIRONMENT_PATH)

IF (GLEW_INCLUDE_DIR AND GLEW_LIBRARY)
   SET(GLEW_FOUND TRUE)
ENDIF (GLEW_INCLUDE_DIR AND GLEW_LIBRARY)
