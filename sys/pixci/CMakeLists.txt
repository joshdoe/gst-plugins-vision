add_definitions (-DHAVE_CONFIG_H)

set (SOURCES
  gstpixcisrc.c)
    
set (HEADERS
  gstpixcisrc.h)

include_directories (AFTER
  ${XCLIB_INCLUDE_DIR})

set (libname libgstpixci)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${XCLIB_LIBRARIES})
  
set (pdbfile "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${libname}.pdb")
install (FILES ${pdbfile} DESTINATION lib/gstreamer-1.0)
install(TARGETS ${libname}
  LIBRARY DESTINATION lib/gstreamer-1.0)