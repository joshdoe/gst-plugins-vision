set (SOURCES
  gstedt.c
  gstedtpdvsink.c
  gstedtpdvsrc.c)

set (HEADERS
  gstedtpdvsink.h
  gstedtpdvsrc.h)

include_directories (AFTER
  ${EDT_INCLUDE_DIR})

set (libname gstedt)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${EDT_LIBRARIES})
  
if (WIN32)
  install (FILES $<TARGET_PDB_FILE:${libname}> DESTINATION ${PDB_INSTALL_DIR} COMPONENT pdb OPTIONAL)
endif ()
install(
  TARGETS ${libname} 
  LIBRARY DESTINATION lib COMPONENT ${PLUGIN_INSTALL_DIR}
  )
