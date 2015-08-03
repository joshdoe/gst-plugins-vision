add_definitions(-DHAVE_CONFIG_H)

set ( SOURCES
    gstphoenixsrc.c )
    
set ( HEADERS
    gstphoenixsrc.h )

include_directories ( AFTER
  ${PHOENIX_INCLUDE_DIR}
  .)

add_library ( libgstphoenix MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries ( libgstphoenix
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_INTERFACE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${PHOENIX_LIBRARIES})
  
install (TARGETS libgstphoenix
  LIBRARY DESTINATION lib/gstreamer-0.10)