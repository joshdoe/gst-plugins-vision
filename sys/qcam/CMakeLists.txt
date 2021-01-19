set (SOURCES
  gstqcamsrc.c)
    
set (HEADERS
  gstqcamsrc.h)

include_directories (AFTER
  ${QCAM_INCLUDE_DIR}
  )

set (libname gstqcam)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

set (LIBRARIES
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${QCAM_LIBRARIES}
  )

target_link_libraries (${libname}
  ${LIBRARIES}
  )

if (WIN32)
  install (FILES $<TARGET_PDB_FILE:${libname}> DESTINATION ${PDB_INSTALL_DIR} COMPONENT pdb OPTIONAL)
endif ()
install(TARGETS ${libname} LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR})
