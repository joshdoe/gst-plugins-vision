add_definitions(-DHAVE_CONFIG_H)

set ( SOURCES
    gstniimaqdx.c )
    
set ( HEADERS
    gstniimaqdx.h )

include_directories ( AFTER
  .
  ${NIIMAQDX_INCLUDE_DIR} )

add_library ( libgstniimaqdx MODULE
  ${SOURCES}
  ${HEADERS} )

target_link_libraries ( libgstniimaqdx
  general ${GLIB2_LIBRARIES}
  general ${GOBJECT_LIBRARIES}
  general ${GSTREAMER_LIBRARY}
  general ${GSTREAMER_BASE_LIBRARY}
  general ${GSTREAMER_VIDEO_LIBRARY}
  general ${NIIMAQDX_LIBRARIES} )
  
install (TARGETS libgstniimaqdx
  LIBRARY DESTINATION lib/gstreamer-0.10)
