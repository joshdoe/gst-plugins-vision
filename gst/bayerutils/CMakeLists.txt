set (SOURCES
  gstbayer2gray.c
  )
    
set (HEADERS
  gstbayer2gray.h)
    
include_directories (AFTER
  )

set (libname gstbayerutils)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})
  
target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY})
  
set (pdbfile "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${libname}.pdb")
install (FILES ${pdbfile} DESTINATION lib/gstreamer-1.0 COMPONENT pdb)
install(TARGETS ${libname}
  LIBRARY DESTINATION lib/gstreamer-1.0)
