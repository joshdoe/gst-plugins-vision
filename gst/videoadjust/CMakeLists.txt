set (SOURCES
  gstvideoadjust.c
  gstvideolevels.c)
    
set (HEADERS
  gstvideolevels.h)

include_directories (AFTER
  ${PROJECT_SOURCE_DIR}/common
  )

set (libname gstvideoadjust)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})
  
target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY})

if (WIN32)
  install (FILES $<TARGET_PDB_FILE:${libname}> DESTINATION ${PDB_INSTALL_DIR} COMPONENT pdb OPTIONAL)
endif ()
install(
  TARGETS ${libname} 
  LIBRARY DESTINATION lib COMPONENT ${PLUGIN_INSTALL_DIR}
  )
