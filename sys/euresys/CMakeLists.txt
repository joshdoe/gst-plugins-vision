set (SOURCES
  gsteuresyssrc.c)
    
set (HEADERS
  gsteuresyssrc.h)

include_directories (AFTER
  ${EURESYS_INCLUDE_DIR})

set (libname gsteuresys)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${EURESYS_LIBRARIES})
  
set (pdbfile "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/lib${libname}.pdb")
install (FILES ${pdbfile} DESTINATION lib/gstreamer-1.0 COMPONENT pdb)
install (TARGETS ${libname}
  LIBRARY DESTINATION lib/gstreamer-1.0)
