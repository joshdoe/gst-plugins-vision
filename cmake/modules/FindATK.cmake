FILE(TO_CMAKE_PATH "$ENV{ATK_DIR}" TRY1_DIR)
FILE(TO_CMAKE_PATH "${ATK_DIR}" TRY2_DIR)
FILE(GLOB ATK_DIR ${TRY1_DIR} ${TRY2_DIR})

FIND_PATH(ATK_INCLUDE_DIR atk/atk.h
                          PATHS ${ATK_DIR}/include /usr/local/include/atk-1.0 /usr/include/atk-1.0
                          ENV INCLUDE DOC "Directory containing atk/atk.h include file")

IF (ATK_INCLUDE_DIR)
  SET(ATK_FOUND TRUE)
ENDIF (ATK_INCLUDE_DIR)
