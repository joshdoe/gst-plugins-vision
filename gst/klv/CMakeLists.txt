set (SOURCES
  gstklv.c
  gstklvinject.c
  gstklvinspect.c)
    
set (HEADERS
  gstklvinject.h
  gstklvinspect.h)

include_directories (AFTER
  ${PROJECT_SOURCE_DIR}/common
  ${PROJECT_SOURCE_DIR}/gst-libs/klv
  )

set (libname gstklv)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})
  
target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  gstklv-1.0-0)

if (WIN32)
  install (FILES $<TARGET_PDB_FILE:${libname}> DESTINATION ${PDB_INSTALL_DIR} COMPONENT pdb OPTIONAL)
endif ()
install(
  TARGETS ${libname} 
  LIBRARY DESTINATION lib COMPONENT ${PLUGIN_INSTALL_DIR}
  )
