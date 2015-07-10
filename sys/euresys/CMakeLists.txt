add_definitions(-DHAVE_CONFIG_H)

set ( SOURCES
    gsteuresyssrc.c )
    
set ( HEADERS
    gsteuresyssrc.h )

include_directories ( AFTER
  .
  ${EURESYS_INCLUDE_DIR} )

add_library ( libgsteuresys MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries ( libgsteuresys
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_INTERFACE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${EURESYS_LIBRARIES})
  
install (TARGETS libgsteuresys
  LIBRARY DESTINATION lib/gstreamer-0.10)