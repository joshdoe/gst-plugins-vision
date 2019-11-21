add_definitions(-D_XKEYCHECK_H)

set (SOURCES
  klv.c
  gstpleora.cpp
  gstpleorasrc.cpp)
    
set (HEADERS
  klv.h
  gstpleorasrc.h)

if (Pleora_VERSION_MAJOR GREATER 5)
  set (SOURCES
    ${SOURCES}
    gstpleorasink.cpp
    streamingchannelsource.cpp)
  set (HEADERS
    ${HEADERS}
    gstpleorasink.h
    streamingchannelsource.h)
endif ()

include_directories (AFTER
  ${Pleora_INCLUDE_DIR})

link_directories(${Pleora_LIBRARY_DIR})

set (libname libgstpleora${Pleora_VERSION_MAJOR})

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY})

set (pdbfile "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${libname}.pdb")
install (FILES ${pdbfile} DESTINATION lib/gstreamer-1.0 COMPONENT pdb)
install(TARGETS ${libname}
  LIBRARY DESTINATION lib/gstreamer-1.0)
