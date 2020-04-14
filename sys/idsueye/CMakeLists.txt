set (SOURCES
  gstidsueyesrc.c)
    
set (HEADERS
  gstidsueyesrc.h)

include_directories (AFTER
  ${IDSUEYE_INCLUDE_DIR})

set (libname libgstidsueye)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})

target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${IDSUEYE_LIBRARIES})

if (WIN32)
  set (pdbfile "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${libname}.pdb")
  install (FILES ${pdbfile} DESTINATION lib/gstreamer-1.0 COMPONENT pdb)
endif (WIN32)
install(TARGETS ${libname}
  LIBRARY DESTINATION lib/gstreamer-1.0)
