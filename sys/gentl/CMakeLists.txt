set (SOURCES
  gstgentlsrc.c
  ioapi.c
  unzip.c)
    
set (HEADERS
  gstgentlsrc.h)

include_directories (AFTER
  ${GSTREAMER_INCLUDE_DIR}/..
  ${PROJECT_SOURCE_DIR}/common
  )

set (libname gstgentl)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${ZLIB_LIBRARIES}
  )

if (WIN32)
  install (FILES $<TARGET_PDB_FILE:${libname}> DESTINATION ${PDB_INSTALL_DIR} COMPONENT pdb OPTIONAL)
endif ()
install(TARGETS ${libname} LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR})
