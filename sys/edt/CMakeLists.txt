add_definitions(-DHAVE_CONFIG_H)

set ( SOURCES
    gstedtpdvsrc.c )
    
set ( HEADERS
    gstedtpdvsrc.h )

include_directories ( AFTER
  .
  ${EDT_INCLUDE_DIR} )

add_library ( libgstedt MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries ( libgsteuresys
  ${GLIB2_LIBRARIES}
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_INTERFACE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${EDT_LIBRARIES})
  
install (TARGETS libgsteuresys
  LIBRARY DESTINATION lib/gstreamer-0.10)