set (SOURCES
  gstpylonsrc.c
  )
    
set (HEADERS
  gstpylonsrc.h)

include_directories (AFTER
  ${GSTREAMER_INCLUDE_DIR}/..
  ${PYLON_INCLUDE_DIR}
  )

set (libname gstpylon)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${PYLON_LIBRARIES}
  )

if (WIN32)
  install (FILES $<TARGET_PDB_FILE:${libname}> DESTINATION ${PDB_INSTALL_DIR} COMPONENT pdb OPTIONAL)
endif ()
install(
  TARGETS ${libname} 
  LIBRARY DESTINATION lib COMPONENT ${PLUGIN_INSTALL_DIR}
  )
