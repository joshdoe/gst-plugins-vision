FILE(TO_CMAKE_PATH "$ENV{NIIMAQ_DIR}" TRY1_DIR)
FILE(TO_CMAKE_PATH "${NIIMAQ_DIR}" TRY2_DIR)
FILE(GLOB NIIMAQ_DIR ${TRY1_DIR} ${TRY2_DIR})

FIND_PATH(NIIMAQ_INCLUDE_DIR niimaq.h
                           PATHS "${NIIMAQ_DIR}/Shared/ExternalCompilerSupport/C/Include" "${NIIMAQ_DIR}/Include"
                           ENV INCLUDE DOC "Directory containing niimaq.h include file")

FIND_LIBRARY(NIIMAQ_LIBRARY NAMES imaq
                          PATHS "${NIIMAQ_DIR}/Shared/ExternalCompilerSupport/C/Lib32/MSVC" "${NIIMAQ_DIR}/Lib32/MSVC"
                          ENV LIB
                          DOC "niimaq library to link with"
                          NO_SYSTEM_ENVIRONMENT_PATH)

IF (NIIMAQ_INCLUDE_DIR AND NIIMAQ_LIBRARY)
   SET(NIIMAQ_FOUND TRUE)
ENDIF (NIIMAQ_INCLUDE_DIR AND NIIMAQ_LIBRARY)
