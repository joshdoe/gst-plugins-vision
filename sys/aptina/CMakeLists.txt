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

if (WIN32)
  install (FILES $<TARGET_PDB_FILE:${libname}> DESTINATION ${PDB_INSTALL_DIR} COMPONENT pdb OPTIONAL)
endif ()
install(
  TARGETS ${libname} 
  LIBRARY DESTINATION lib COMPONENT ${PLUGIN_INSTALL_DIR}
  )
