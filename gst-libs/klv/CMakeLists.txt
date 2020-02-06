add_definitions(-DBUILDING_GST_KLV)

set (SOURCES
  klv.c)
    
set (HEADERS
  klv.h)

include_directories (AFTER
  ${PROJECT_SOURCE_DIR}/common
  )

set (libname libgstklv-1.0-0)

add_library (${libname} SHARED
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
install (TARGETS ${libname}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib)