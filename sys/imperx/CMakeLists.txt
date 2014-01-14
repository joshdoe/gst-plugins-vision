add_definitions(-DHAVE_CONFIG_H)

set ( SOURCES
    gstframelinksrc.c )
    
set ( HEADERS
    gstframelinksrc.h )

include_directories ( AFTER
  ${FRAMELINK_INCLUDE_DIR}
  .)

add_library ( libgstframelink MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries ( libgstframelink
  ${GLIB2_LIBRARIES}
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_INTERFACE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${FRAMELINK_LIBRARIES})
  
install (TARGETS libgstframelink
  LIBRARY DESTINATION lib/gstreamer-0.10)
