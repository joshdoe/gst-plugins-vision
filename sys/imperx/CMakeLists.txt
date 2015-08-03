add_definitions(-DHAVE_CONFIG_H)

set ( SOURCES
    gstframelinksrc.c )
    
set ( HEADERS
    gstframelinksrc.h )

include_directories ( AFTER
  ${IMPERX_FLEX_INCLUDE_DIR}
  .)

add_library ( libgstimperxflex MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries ( libgstimperxflex
  ${GLIB2_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GSTREAMER_LIBRARY}
  ${GSTREAMER_BASE_LIBRARY}
  ${GSTREAMER_INTERFACE_LIBRARY}
  ${GSTREAMER_VIDEO_LIBRARY}
  ${IMPERX_FLEX_LIBRARIES})
  
install (TARGETS libgstimperxflex
  LIBRARY DESTINATION lib/gstreamer-1.0)
