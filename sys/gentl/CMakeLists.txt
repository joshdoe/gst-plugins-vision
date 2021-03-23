set (SOURCES
  gstgentlsrc.c
  ioapi.c
  unzip.c)
    
set (HEADERS
  gstgentlsrc.h)

include_directories (AFTER
  ${GSTREAMER_INCLUDE_DIR}/..
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
  ${GSTREAMER_INCLUDE_DIR}/../../lib/z.lib
  )

set (pdbfile "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${libname}.pdb")
install (FILES ${pdbfile} DESTINATION lib/gstreamer-1.0 COMPONENT pdb)
install(TARGETS ${libname}
  LIBRARY DESTINATION lib/gstreamer-1.0)
