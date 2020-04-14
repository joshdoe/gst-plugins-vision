add_definitions(-DHAVE_ORC=TRUE)

set (SOURCES
  gstaptinasrc.c)
    
set (HEADERS
  gstaptinasrc.h)

include_directories (AFTER
  ${ORC_INCLUDE_DIR}
  ${APTINA_INCLUDE_DIR})

set (libname gstaptina)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries (${libname}
  ${ORC_LIBRARIES}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${APTINA_LIBRARIES})

set (pdbfile "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/lib${libname}.pdb")
install (FILES ${pdbfile} DESTINATION lib/gstreamer-1.0 COMPONENT pdb)
install(TARGETS ${libname}
  LIBRARY DESTINATION lib/gstreamer-1.0)
