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

set (libname libgstklv)

add_library (${libname} MODULE
  ${SOURCES}
  ${HEADERS})
  
target_link_libraries (${libname}
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  libgstklv-1.0-0)

set (pdbfile "${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${libname}.pdb")
install (FILES ${pdbfile} DESTINATION lib/gstreamer-1.0 COMPONENT pdb)
install (TARGETS ${libname}
    LIBRARY DESTINATION lib/gstreamer-1.0)